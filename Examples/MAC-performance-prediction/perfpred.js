module.exports = function (RED) {
    function perfpred(config) {
        var spawn = require('child_process').spawn;
        this.cmd = "";
        this.cmd_args = [];

        RED.nodes.createNode(this, config);
        var node = this;
        node.on('input', function (msg) {
            var input_params = msg.payload.input_params;
            this.cmd_args = [__dirname + "\\MACperf.py", input_params];

            var flowContext = this.context().flow;
            this.cmd = flowContext.get('pythonpath');

            child = spawn(this.cmd, this.cmd_args);

            child.stdout.on('data', function (data) {
                msg.payload = data.toString();
                node.send(msg);
            });
            child.stderr.on('data', function (data) {
                console.log('[perfpred] stderr: ' + data);
                msg.payload = data.toString();
                node.error(msg);
            });
            child.on('close', function (code) {
                if (code == 0) { node.status({}); }
                if (code === null) { node.status({ fill: "red", shape: "dot", text: "timeout" }); }
                else if (code < 0) { node.status({ fill: "red", shape: "dot", text: "rc: " + code }); }
                else { node.status({ fill: "green", shape: "dot", text: "rc: " + code }); }
            });
            child.on('error', function (code) {
                if (String(code).endsWith("ENOENT")) {
                    msg.payload = "Something is wrong with the provided paths. Is the Python path correctly specified?\n";
                    msg.payload = msg.payload + code;
                    node.error(code, msg);
                }
                else {
                    node.error(code, msg);
                }
            });

        });
        this.on('close', function () {
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
    RED.nodes.registerType("perfpred", perfpred);
}