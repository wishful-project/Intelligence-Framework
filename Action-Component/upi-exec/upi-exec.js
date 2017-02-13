module.exports = function(RED) {

  function UPIExec(config) {
    RED.nodes.createNode(this, config);
    var node = this;
    this.upitype = config.upitype;
    this.func = config.func;
    this.paramname = config.paramname || 'no-param';
    this.nodes = config.nodes || []
    this.paramval = config.paramval || '';
 
 /*
 def parse_node_red_command(json_client, nr_command, node_manager):
	if 'execute_upi_function' in nr_command:
		nr_exec_upi_func = nr_command['execute_upi_function']
		upi_type = nr_exec_upi_func['upi_type']
		upi_func = nr_exec_upi_func['upi_func']
		node_list = nr_exec_upi_func['node_list']
		upi_func_args = nr_exec_upi_func['args']
		ret_val = node_manager.execute_upi_function(upi_type, upi_func, node_list,args=upi_func_args)
		json_ret = { 'execute_upi_function' : {'upi_type': upi_type, "upi_func": upi_func, "node_list": node_list, 'ret_val' : ret_val}}
		json_client.send_obj(json_ret)
		
	{ 'execute_upi_function' :
    {'upi_type': "radio", 'upi_func": set_parameters, "node_list": [1,2,3], 'args' : {'IEEE802154_phyCurrentChannel':12}}
}	


[ 123, 34, 112, 97, 121, 108, 111, 97, 100, 34, 58, 32, 123, 34, 101, 120, 101, 99, 117, 116, 101, 95, 117, 112, 105, 95, 102, 117, 110, 99, 116, 105, 111, 110, 34, 58, 32, 123, 34, 117, 112, 105, 95, 116, 121, 112, 101, 34, 58, 32, 34, 114, 97, 100, 105, 111, 34, 44, 32, 34, 114, 101, 116, 95, 118, 97, 108, 34, 58, 32, 123, 125, 44, 32, 34, 110, 111, 100, 101, 95, 108, 105, 115, 116, 34, 58, 32, 34, 91, 49, 44, 50, 93, 34, 44, 32, 34, 117, 112, 105, 95, 102, 117, 110, 99, 34, 58, 32, 34, 115, 101, 116, 95, 112, 97, 114, 97, 109, 101, 116, 101, 114, 34, 125, 125, 125 ]


*/
 
    this.on('input', function(msg) {
		//msg.payload = {'wishful' : []};
//??		JSON.stringify
		
		msg.payload = {
				'execute_upi_function' : {
					'upi_type' : this.upitype,
					'upi_func' : this.func,
					'node_list': JSON.parse(this.nodes),
					'args' : { 
//						pn : this.paramval 
					}
				}
		};
		
		if (this.func == "get_parameters"){
			msg.payload['execute_upi_function']['args'] = [ this.paramname ];
		} else if (this.func == "set_parameters"){
			msg.payload['execute_upi_function']['args'][this.paramname] = parseInt(this.paramval);
		} else {
			console.error("Unknown func: ", this.func);
		}
		
		console.log("sending wishful payload: ", JSON.stringify(msg.payload));
        node.send(msg);
    });
    
    node.on('close', function() {
		console.log('closed upi exec');
    });
  }
  RED.nodes.registerType("wishful-upi-exec", UPIExec);
   
}
