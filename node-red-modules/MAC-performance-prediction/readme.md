# MAC performance prediction

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
See examples for usage of this module and a proof-of-concept for MAC selection.