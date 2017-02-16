/**
 * Copyright 2016 IBM Corp.
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

	// Convert integer to hexadecimal of given size
	function int2hex(integer, size)
	{
		// Is the passed value an integer
		if(Number.isInteger(integer) === true)
		{
			var buf = Buffer.allocUnsafe(4);
			// 8 bit integer
			if(size == 1)
			{
				var uint8 = new Uint8Array([integer]);
				buf.writeUInt8(uint8[0], 0);
			}
			// 16 bit integer
			else if(size == 2)
			{
				var uint16 = new Uint16Array([integer]);
				buf.writeUInt16LE(uint16[0], 0);
			}
			// Else, use 32 bit integer
			else
			{
				size = 4;
				var uint32 = new Uint32Array([integer]);
				buf.writeUInt32LE(uint32[0], 0);
			}
			return buf.toString('hex', 0, size);
		}
		else
		{
			console.error("int2hex: " + integer + " is not an integer");
		}
	}

	function exhaustive_search(n)
	{
		RED.nodes.createNode(this,n);

		// Global variable definition
		this.ptsFiles = [];
		this.PUT_ctrl_hdr;
		this.sampleSet_data = [];

		var node = this;

		this.on("input", function(msg)
		{
			node.status({fill:"green", shape:"dot", text:"executing"});

			// Start experiment
			if(msg.topic == "start_experiment")
			{
				// Global variable initialization
				for(var i = 0; i < msg.PUT.GROUPs.length; i++)
				{
					var GROUP = msg.PUT.GROUPs[i];
					for(var j = 0; j < GROUP.length; j++)
						node.ptsFiles.push(GROUP[j].ptsFile);
				}
				node.PUT_ctrl_hdr = msg.PUT.control_hdr;

				// Array of parameter values and iterators
				var values_array = [];
				var values_iter = [];
				for(var i = 0; i < node.PUT_ctrl_hdr.length; i++)
				{
					// Sort values in ascending order
					var values = node.PUT_ctrl_hdr[i].values.sort(function(a, b){return a - b;});
					values_array[i] = values;
					values_iter[i]  = 0;
				}

				// Construct exhaustive search sampleSet data
				outer_loop:
				for(;;)
				{
					// Construct one sampleSet at the given iterators
					var sampleSet = [];
					for(var i = 0; i < values_array.length; i++)
					{
						var values = values_array[i];
						var iter = values_iter[i];

						sampleSet[i] = values[iter];
					}
					node.sampleSet_data.push(sampleSet);

					// Update parameter values iterator
					for(var i = 0; i < values_array.length; i++)
					{
						var values = values_array[i];
						var iter = values_iter[i];

						// If we are not at the end, increament the iterator
						if(iter < (values.length-1))
						{
							values_iter[i] = iter + 1;
							continue outer_loop;
						}
						// If we reached the end, flip over to the next values
						else
						{
							values_iter[i] = 0;
						}
					}

					// sampleSet data covered all the design space. Time to leave forever loop
					break;
				}

				// Initial sample index
				var sample_Idx = 0;

				// Populate design parameters
				var parameters = [];
				for (var i = 0; i < node.ptsFiles.length; i++)
				{
					// Retrieve sampleSet
					var sampleSet = node.sampleSet_data[sample_Idx].slice(0);
					sampleSet = sampleSet.map(function(x){return Math.round(x)});

					var control_hdr = [];
					for(var j = 0; j < sampleSet.length; j++)
					{
						var uid = node.PUT_ctrl_hdr[j].uid;
						var type = node.PUT_ctrl_hdr[j].type;
						var len = node.PUT_ctrl_hdr[j].len;
						var value = int2hex(sampleSet[j], len);

						control_hdr.push({uid: uid, type: type, len: len, value : value});
					}

					parameters.push({ ptsFile : node.ptsFiles[i], opcode : 1, control_hdr : control_hdr});
				}

				// After sampleSet messages are [handler] saved, send configuration messages to the next block
				msg.payload = parameters;
				node.send([msg, null]);
			}
			else
			{
				// If message does not contain sample_Idx field
				if(typeof(msg.sample_Idx) === "undefined")
				{
					msg.payload = "exhaustive-search: undefined sample_Idx field";
					node.send([null, msg]);
				}

				// Retreive sample index
				var sample_Idx = msg.sample_Idx;

				// Finished executing exhaustive sampleSet data
				if(sample_Idx < node.sampleSet_data.length)
				{
					// Message topic
					if(sample_Idx === (node.sampleSet_data.length - 1))
						msg.topic = "planning_finished";
					else
						msg.topic = "exploration";

					// Populate design parameters
					var parameters = [];
					for (var i = 0; i < node.ptsFiles.length; i++)
					{
						// Retrieve sampleSet
						var sampleSet = node.sampleSet_data[sample_Idx].slice(0);
						sampleSet = sampleSet.map(function(x){return Math.round(x)});

						var control_hdr = [];
						for(var j = 0; j < sampleSet.length; j++)
						{
							var uid = node.PUT_ctrl_hdr[j].uid;
							var type = node.PUT_ctrl_hdr[j].type;
							var len = node.PUT_ctrl_hdr[j].len;
							var value = int2hex(sampleSet[j], len);

							control_hdr.push({uid: uid, type: type, len: len, value : value});
						}
						parameters.push({ ptsFile : node.ptsFiles[i], opcode : 1, control_hdr : control_hdr});
					}
					msg.payload = parameters;

					// After sampleSet messages are [handler] saved, send configuration messages to the next block
					node.send([msg, null]);
				}
				else
				{
					msg.payload = "exhaustive-search: sample_Idx exceeded the limit";
					node.send([null, msg]);
				}
			}
			node.status({});
		});
	}
	RED.nodes.registerType("exhaustive-search",exhaustive_search);
}
