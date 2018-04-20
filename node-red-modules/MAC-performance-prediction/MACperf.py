import sys
import os
import numpy as np
from sklearn.externals import joblib
from sklearn.neural_network import MLPRegressor

# read input parameters
input_str = sys.argv[1:]
input_params = np.array(input_str[0].split(','))
input_params = np.array([input_params]).astype(np.float)

# check for 6 parameters
if (len(input_params[0]) != 6):
    sys.stderr.write(
        'Model requires 6 input parameters, instead only {0} were given.'.
        format(len(input_params)))
    sys.exit()

# load neural network
os.chdir(os.path.dirname(__file__))
nn = joblib.load(os.getcwd() + '\\predictionmodel.pkl')

# perform MAC performance prediction
print(nn.predict(input_params))