from tensorflow.python.keras.models import Sequential
from tensorflow.python.keras.layers import Dense
from tensorflow.python.keras.utils import to_categorical
import numpy as np

from enum import Enum
class Label(Enum) :
    SILENCE = 0
    WHISTLE = 1
    CLAP = 2

np.random.seed(7)

# load dataset
dataset = np.loadtxt("clap_whistle_silence_data.csv", delimiter=';')

# shuffle dataset for independent results
np.random.shuffle(dataset)

# prepare dataset for training
x_samples = dataset[:, 0:512]  # fft data
enumerate_samples = dataset[:, 512]    # class

y_samples = to_categorical(enumerate_samples, num_classes=3) # transform enum in cathegorical data -> one-hot cod.

#model
model = Sequential()
model.add(Dense(10, input_dim=512, activation='relu', kernel_initializer='random_uniform')) # kernel_init because CUbe.ai does not recognize GlorotUniform distribution
#model.add(Dense(5, activation='relu'))      # may be commented for smaller net
model.add(Dense(3, activation='softmax', kernel_initializer='random_uniform'))   # softmax -> classificator
#end model

model.compile(loss='categorical_crossentropy', optimizer='adam', metrics=['accuracy'])

# training
model.fit(x_samples, y_samples, epochs=50, batch_size=10)

# evaluate the network with the samples used for training
scores = model.evaluate(x_samples, y_samples)   # optimistic metrics
print("\n%s: %.2f%%" % (model.metrics_names[1], scores[1]*100))

# evaluate the network with test samples unused for training
testset = np.loadtxt("test_cws.csv", delimiter=';')
x_test = dataset[:, 0:512]
y_test = to_categorical(dataset[:,512], num_classes=3)
scores = model.evaluate(x_test, y_test)     # real metrics
print("\n%s: %.2f%%" % (model.metrics_names[1], scores[1]*100))

# save weights and description of the NN in a file
# model.h to be compiled in Cube.ai
#model.save("model.h5", True, True)  # first True to overwrite file if existing
                                    # second True to not save optimizer

#model.save_weights("weights.h5")

model_json = model.to_json()
with open("model.json", "w") as json_file:
    json_file.write(model_json)
model.save_weights("weights_2.h5")