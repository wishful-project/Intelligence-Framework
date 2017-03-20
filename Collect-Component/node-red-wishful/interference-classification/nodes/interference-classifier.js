module.exports = function(RED) {
    function LowerCaseNode(config) {
        RED.nodes.createNode(this,config);
        var node = this;
       
        this.on('input', function(msg) {
             // msg.payload = msg.payload.toLowerCase();
            // msg.payload = 'no interference';
            node.status({fill:"green", shape:"dot", text:"executing"});
			
			var process = require('child_process');
			// (/Applications/MATLAB_R2015b.app/bin/matlab -nodisplay -nodesktop -r "try; prova; catch; end; quit" ) > YourOutputLogFile 2> YourErrorLogFile

			process.exec('(/Applications/MATLAB_R2015b.app/bin/matlab -nodisplay -nodesktop -r "try; high_density_one_shot; catch; end; quit" ) > wishful-interference.log 2> wishful-interference-err.log',function (err,stdout,stderr) {
			    if (err) {
			        console.log("\n"+stderr);
			    } else {
			        console.log(stdout);
			    }
			});
            
            if(msg.payload==1)
    			msg.payload = 'no interference';
    		if(msg.payload==2)
    			msg.payload = 'collision';
    		if(msg.payload==3)
    			msg.payload = 'refrained';
			node.send(msg);

        });
    }
    RED.nodes.registerType("interference-classifier",LowerCaseNode);
}