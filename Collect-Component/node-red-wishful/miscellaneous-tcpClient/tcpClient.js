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

	function tcpClient(n)
	{
		RED.nodes.createNode(this,n);
		this.host = n.host;
		this.port = n.port * 1;
		this.client_disconnects = n.client_disconnects;
		var node = this;

		var buffer = "";
		this.on('input', function(msg)
		{
			var topic = msg.topic;
			var client = net.connect(node.port, node.host, function()
			{
				// Send stringified message
				client.write(JSON.stringify(msg));
			});
			client.on('data', function (data)
			{
				buffer = buffer + data;

				// Client starts disconnection
				if(node.client_disconnects == true)
				{
					var newline_pos = buffer.indexOf('\n');
					if(newline_pos != -1)
					{
						var msg = {payload: buffer.substring(0, newline_pos+1), topic: topic};
						node.send(msg);

						// Close connection
						client.destroy();
					}
				}
			});
			client.on('close', function()
			{
				// Server starts disconnection
				if(node.client_disconnects == false)
				{
					var msg = {payload: buffer.toString(), topic: topic};
					node.send(msg);
				}
			});
			client.on('error', function(err)
			{
				node.log(err);
			});
		});
	}
	RED.nodes.registerType("tcpClient",tcpClient);
}
