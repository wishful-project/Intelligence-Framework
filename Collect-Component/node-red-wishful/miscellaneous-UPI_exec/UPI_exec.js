/**
 * Copyright 2013,2015 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

module.exports = function(RED)
{
	"use strict";
	var net = require('net');

	// Send payload, error and topic message
	function send_msg(node)
	{
		var payload	= node.payload;
		var topic	= node.topic;
		var error_msg	= [];

		for(var i = 0; i < payload.length; i++)
		{
			var resp = payload[i].response;

			// PARAM_ERROR. Splice the request/response from the payload
			if(resp.opcode == 127)
			{
				error_msg.push(payload.splice(i, 1)[0]);
				i--;
			}
		}
		node.status({});
		node.send({payload: payload, error_msg: error_msg, topic: topic});
	}

	// Request socket call to pts multiplexer
	function pts_mux_call(node, req, ptsFile)
	{
		// Create a TCP socket
		var socket = new net.Socket();

		// Connect to the server and send the message
		socket.connect(node.port, node.host, function()
		{
			// Send stringified message
			socket.write(JSON.stringify({ptsFile: ptsFile, request: req.data_base64}));

			// Configure request timeout and watch it over
			socket.setTimeout(node.timeout);
			socket.on('timeout', function()
			{
				// Request has timed out
				node[ptsFile].error_msg.push("request timeout");

				// Decrease retry limit
				node[ptsFile].retry--;

				// If retry limit has not reached
				if(node[ptsFile].retry > 0)
				{
					// Re-start the pts multiplexer call
					setTimeout(pts_mux_call, 0, node, req, ptsFile);
				}
				// If retry limit has reached, notify user about the failure
				else
				{
					var resp = {opcode: 127, error_msg: node[ptsFile].error_msg};
					node.payload.push({request: req, response: resp});

					// Did we receive all message responses? If so, send the message
					if(node.payload.length === node.req_count)
						send_msg(node);
				}

				// Close connection
				socket.destroy();
			});
		});
		socket.on('data', function (resp_base64)
		{
			var data	= Buffer.from(resp_base64.toString(), 'base64');
			var opcode	= data.readUInt8(0);
			var num_param	= data.readUInt8(1);
			var seq_nr	= data.readUInt32LE(2);

			// PARAM_ERROR
			if(opcode == 127)
			{
				// Error is always sent in one message container
				var len = data.readUInt8(9);
				var value = data.slice(10, 10 + len).toString();
				node[ptsFile].error_msg.push(value);

				// Decrease retry limit
				node[ptsFile].retry--;

				// If retry limit has not reached
				if(node[ptsFile].retry > 0)
				{
					// Re-start the pts multiplexer call
					setTimeout(pts_mux_call, 0, node, req, ptsFile);
				}
				// If retry limit has reached, notify user about the failure
				else
				{
					// Store response into the message holder
					var resp = {opcode: opcode, error_msg: node[ptsFile].error_msg};
					node.payload.push({request: req, response: resp});

					// Did we receive all message responses? If so, send the message
					if(node.payload.length === node.req_count)
						send_msg(node);
				}
			}
			else
			{
				// Format control header message
				var control_hdr = [];
				var offset = 6;
				for(var i = 0; i < num_param; i++)
				{
					var uid   = data.readUInt16LE(offset);
					var type  = data.readUInt8(offset + 2);
					var len   = data.readUInt8(offset + 3);
					var value = data.slice(offset + 4, offset + 4 + len).toString('hex');

					control_hdr.push({uid: uid, type: type, len: len, value: value});

					offset = offset + 4 + len;
				}

				var resp = {opcode: opcode, num_param: num_param, seq_nr: seq_nr, control_hdr: control_hdr};
				node.payload.push({request: req, response: resp});

				// Did we receive all message responses? If so, send the message
				if(node.payload.length === node.req_count)
					send_msg(node);
			}

			// Close connection
			socket.destroy();
		});
		socket.on('error', function(err)
		{
			node[ptsFile].error_msg.push(err);

			// Decrease retry limit
			node[ptsFile].retry--;

			// If retry limit has not reached
			if(node[ptsFile].retry > 0)
			{
				// Re-start the pts multiplexer call
				setTimeout(pts_mux_call, 0, node, req, ptsFile);
			}
			// If retry limit has reached, notify user about the failure
			else
			{
				// Store response into the message holder
				var resp = {opcode: 127, error_msg: node[ptsFile].error_msg};
				node.payload.push({request: req, response: resp});

				// Did we receive all message responses? If so, send the message
				if(node.payload.length === node.req_count)
					send_msg(node);
			}

			// Close connection
			socket.destroy();
		});
	}

	function UPI_exec(n)
	{
		RED.nodes.createNode(this,n);
		this.host	= n.host;
		this.port	= parseInt(n.port);
		this.timeout	= parseInt(n.timeout);
		this.retry	= parseInt(n.retry);

		var node = this;

		this.on('input', function(msg)
		{
			// cooja execution status
			node.status({fill:"green", shape:"dot", text:"executing"});

			// Initialize parameters
			node.req_count	= msg.payload.length;
			node.topic	= msg.topic;
			node.payload	= [];

			// Each message request is sent to respective nodes separately
			for(var i = 0; i < node.req_count; i++)
			{
				var req	= msg.payload[i];

				var ptsFile	= req.ptsFile;
				var opcode	= req.opcode;
				var control_hdr = req.control_hdr;
				var num_param	= control_hdr.length;

				// status [retry and error_msg] variables
				node[ptsFile] = {retry: node.retry, error_msg: []};

				// Calculate buffer size
				var buffer_size = 6;
				for(var j = 0; j < num_param; j++)
				{
					// PARAM_SET or EVENT_REGISTER
					if(opcode == 1 || opcode == 2)
					{
						var len = control_hdr[j].len;
						buffer_size = buffer_size + 4 + len;
					}
					// PARAM_GET and EVENT_PUSH
					else
					{
						buffer_size = buffer_size + 4;
					}
				}

				// Fill in buffer
				var data = Buffer.allocUnsafe(buffer_size);
				data.writeUInt8(opcode, 0);
				data.writeUInt8(num_param, 1);
				data.writeUInt32LE(0, 2);

				var offset = 6;
				for(var j = 0; j < num_param; j++)
				{
					var uid	 = control_hdr[j].uid;
					var type = control_hdr[j].type;
					var len	 = control_hdr[j].len;

					data.writeUInt16LE(uid, offset);
					data.writeUInt8(type, offset + 2);
					data.writeUInt8(len, offset + 3);

					// PARAM_SET or EVENT_REGISTER
					if(opcode == 1 || opcode == 2)
					{
						data.write(control_hdr[j].value, offset + 4, len, 'hex');

						offset = offset + 4 + len;
					}
					// PARAM_GET and EVENT_PUSH
					else
					{
						offset = offset + 4;
					}
				}
				req.data_base64 = data.toString('base64', 0, buffer_size);

				// Start the pts multiplexer call
				setTimeout(pts_mux_call, 0, node, req, ptsFile);
			}
		});
	}
	RED.nodes.registerType("UPI_exec",UPI_exec);
}
