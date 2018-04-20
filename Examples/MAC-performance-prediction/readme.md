# MAC performance prediction

This example can be run as follows: 

```
node-red flowMACSelection.json
```

## Requirements
**Python**

<ul>
	<li>Python3</li>
	<li>numpy</li>
	<li>sklearn</li>
</ul>

**node-red "Perfpred" module**

Install the module as follows:
```
npm install C:\pathto\Intelligence-Framework\node-red-modules\MAC-performance-prediction
```

## Intelligence module
The intelligence module predicts the MAC performance of ContikiMAC and CSMA given a description of the environment. This description requires the following metrics:
<ul>
        <li># nodes</li>
        <li>Average power to sink</li>
        <li>Average power to distruber</li>
        <li>Power between sink and distruber</li>
        <li>Message sending interval (sec)</li>
        <li>Interference duty cycle (%)</li>
</ul>

Power can be seen as a average theoretical signal strength or distance between nodes. It should be normalised between 0 and 60, where 0 is very close (maximum power) and 60 is far (minimum power).
A disturber should also be present, indicating an external form of interference. The amount of interference is set in the last input parameter. Message sending interval describes the average quantity of traffic in the network. 


Output parameters (for each MAC protocol):
<ul>
    <li>Latency (ms)</li>
    <li>Packet error rate (%)</li>
    <li>Sink energy (mJ)</li>
    <li>Avg. source energy (mJ)</li>
</ul>

This example illustrates some important configuration needed to use the node-red module "Perfpred". The path to run Python must be set as follows:

```javascript
var flowContext = context.flow;
var python_path = "C:\\pathto\\python.exe";
flowContext.set('pythonpath',python_path);
```

Input parameters must be passed in the payload variable as a "input_parameters" property:

```javascript
{"input_params":[1,19.386593,3.6,18.35647,6,30]}
```
Finnaly, as a proof-of-concept for MAC selection, a function called "MAC selection" is also presented in this example. This function will select the best performing MAC protocol based on the prediction performance metrics and the performance function. The weights of this function are initialized at the top of this function. The output of this module should look like the example below.

```
Select MAC: CSMA
Score CONTIKIMAC vs CSMA: 33.2460868 vs 12.22197838
Results CONTIKIMAC: {"LAT":281.81393177,"PER":33.2460868,"ENGSink":21.11311609,"ENGSource":24.72694432}
Results CSMA: {"LAT":31.73929926,"PER":12.22197838,"ENGSink":412.24438319,"ENGSource":411.89517067}
```