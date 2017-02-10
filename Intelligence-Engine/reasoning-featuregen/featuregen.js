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

module.exports = function(RED) {
    //"use strict";
    var spawn = require('child_process').spawn;

    function FeatureGeneration(n) {
        RED.nodes.createNode(this,n);
        var node = this;

        this.dbdir = (n.dbdir || "").trim();
        this.cmd = "/usr/bin/python2.7";
        this.cmd_args = [];
        
        this.timer = Number(n.timer || 0)*1000;
        
        this.activeProcesses = {};
        
        var cleanup = function(p) {
            //console.log("CLEANUP!!!",p);
            node.activeProcesses[p].kill();
            node.status({fill:"red",shape:"dot",text:"timeout"});
            node.error("Exec node timeout");
        }

        this.on("input", function(msg) {
            var child;
            node.status({fill:"blue",shape:"dot",text:" "});

            this.cmd_args = [ __dirname + "/MACperfFeatureGenerator.py", "-d" ];
            
            // slice whole line by spaces (trying to honour quotes);
            var arg = this.dbdir.match(/(?:[^\s"]+|"[^"]*")+/g);
            arg = arg.shift(); // we take the first one, ignore everything if it has spaces...
            if(arg.slice(-1) != "/"){
                arg = arg + "/"; // add slash at the end
            }
            this.cmd_args.push(arg);

            if (RED.settings.verbose) { 
                console.log("Spawning cmd : ", this.cmd, this.cmd_args); 
            }

            child = spawn(this.cmd, this.cmd_args); // spawn expects only a single cmd and an array of args
            if (node.timer !== 0) {
                child.tout = setTimeout(function() { cleanup(child.pid); }, node.timer);
            }
            node.activeProcesses[child.pid] = child;
            child.stdout.on('data', function (data) {
                console.log('[featuregen] stdout: ' + data);
                // if not utf8 output.. msg.payload = new Buffer(data);
                msg.payload = data.toString();
                node.send([msg,null,null]);
            });
            child.stderr.on('data', function (data) {
                console.log('[featuregen] stderr: ' + data);
                msg.payload = data.toString();
                node.send([null,msg,null]);
            });
            child.on('close', function (code) {
                console.log('[featuregen] result: ' + code);
                delete node.activeProcesses[child.pid];
                if (child.tout) { clearTimeout(child.tout); }
                msg.payload = code;
                if (code === 0) { node.status({}); }
                if (code === null) { node.status({fill:"red",shape:"dot",text:"timeout"}); }
                else if (code < 0) { node.status({fill:"red",shape:"dot",text:"rc: "+code}); }
                else { node.status({fill:"yellow",shape:"dot",text:"rc: "+code}); }
                node.send([null,null,msg]);
            });
            child.on('error', function (code) {
                delete node.activeProcesses[child.pid];
                if (child.tout) { clearTimeout(child.tout); }
                node.error(code,msg);
            });
        });           
        this.on('close',function() {
            for (var pid in node.activeProcesses) {
                if (node.activeProcesses.hasOwnProperty(pid)) {
                    if (node.activeProcesses[pid].tout) { clearTimeout(node.activeProcesses[pid].tout); }
                    node.activeProcesses[pid].kill();
                }
            }
            node.activeProcesses = {};
            node.status({});
        });
    }
    RED.nodes.registerType("Feature-Generation",FeatureGeneration);
}
