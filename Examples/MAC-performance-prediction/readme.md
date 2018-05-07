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
The intelligence module predicts the MAC performance of ContikiMAC, CSMA and TDMA given a description of the environment. This description requires the following metrics:
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

## Example
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
Select MAC: TDMA
Score CONTIKIMAC vs CSMA vs TDMA: 4112.46695991 vs 1731.64740347 vs 1304.31374567
Results CONTIKIMAC: {"LAT":288.65989175,"PER":23.57554087,"ENGSink":22.34641792,"ENGSource":24.74458099}
Results CSMA: {"LAT":31.61354421,"PER":11.85506207,"ENGSink":412.05372759,"ENGSource":410.70513028}
Results TDMA: {"LAT":34.73715921,"PER":17.88125665,"ENGSink":40.29786056,"ENGSource":22.58146051}
```
