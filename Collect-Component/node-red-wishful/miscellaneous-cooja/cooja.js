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
	var spawn = require('child_process').spawn;
	var exec = require('child_process').exec;

	// Retrieve base name of a path
	function baseName(path)
	{
		var base = path.substring(path.lastIndexOf('/') + 1); 
		if(base.lastIndexOf(".") != -1)       
			base = base.substring(0, base.lastIndexOf("."));
		return base;
	}

	// Retrieve directory name of a path
	function dirName(path)
	{
		var dir = "";
		if(path.lastIndexOf('/') != -1)
			dir = path.substring(0, path.lastIndexOf('/'));
		else
			dir = path;
		return dir;
	}

	function cooja(n)
	{
		RED.nodes.createNode(this,n);
		this.restart = n.restart;
		var node = this;

		this.on("input", function(msg)
		{
			node.status({fill:"green", shape:"dot", text:"executing"});

			// Is cooja application running? We will use the Java Virtual Machine Process Status tool (JPS)
			node.jps_child = exec("jps | grep cooja.jar", {encoding: 'binary', maxBuffer:1000}, function (error, stdout, stderr)
			{
				var jps_status = stdout.trim().split(" ");

				// If cooja is running and force restart is enabled, kill the process
				if(jps_status.length === 2 && node.restart == true)
					process.kill(jps_status[0], 'SIGKILL');

				// Cooja parameters
				var CONTIKI_DIR = msg.payload.CONTIKI_DIR;
				var app_path_rel = msg.payload.app_path_rel;
				var app_path_basename = baseName(app_path_rel);
				var app_path_dirname = dirName(app_path_rel);
				var cooja_csc_path = CONTIKI_DIR + "/" + app_path_dirname + "/" + app_path_basename + ".csc";

				// Cooja program arguments
				var arg = " -mx512m -jar " + CONTIKI_DIR + "/tools/cooja/dist/cooja.jar -nogui=" + cooja_csc_path + " -contiki=" + CONTIKI_DIR;
				arg = arg.match(/(?:[^\s"]+|"[^"]*")+/g);

				// Start cooja program
				node.cooja_child = spawn("java", arg);

				// Cooja sends normal data
				node.cooja_child.stdout.on('data', function (data)
				{
					// buf2str
					data = data.toString();

					// Is cooja simulation started
					if(data.indexOf("Simulation main loop started") !== -1)
					{
						node.status({});
						node.send([{payload: "cooja simulator started", topic: msg.topic}, null]);
					}
				});

				// Cooja sends error data
				node.cooja_child.stderr.on('data', function (data)
				{
					var msg = {payload: data.toString()};

					node.status({fill:"red", shape:"dot", text:"Cooja stderr"});
					node.send([null, msg]);
				});

				node.cooja_child.on('close', function (code)
				{
					node.cooja_child = null;
				});

				// Cooja execution error
				node.cooja_child.on('error', function (err)
				{
					if(err.errno === "ENOENT")
						node.warn('Cooja: command not found');
					else if(err.errno === "EACCES")
						node.warn('Cooja: command not executable');
					else
						node.log('Cooja: error: ' + err);
				});

			});

			// JPS execution error
			node.jps_child.on('error', function (err)
			{
				if(err.errno === "ENOENT")
					node.warn('JPS: command not found');
				else if(err.errno === "EACCES")
					node.warn('JPS: command not executable');
				else
					node.log('JPS: error: ' + err);
			});
		});
	}
	RED.nodes.registerType("cooja",cooja);
}
