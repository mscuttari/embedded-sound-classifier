from keras.models import Sequential
from keras.layers import Dense
from keras.utils import to_categorical
import numpy as np

from enum import Enum
class Label(Enum) :
    SILENCE = 0
    WHISTLE = 1
    CLAP = 2

np.random.seed(7)

# load dataset
dataset = np.loadtxt("clap_whistle_silence_data.csv", delimiter=';')

x_samples = dataset[:, 0:512]  # fft data
enumerate_samples = dataset[:, 512]    # class

y_samples = to_categorical(enumerate_samples, num_classes=3)

model = Sequential()
model.add(Dense(8, input_dim=512, activation='relu'))
model.add(Dense(5, activation='relu'))
model.add(Dense(3, activation='softmax'))

model.compile(loss='categorical_crossentropy', optimizer='adam', metrics=['accuracy'])

model.fit(x_samples, y_samples, epochs=50, batch_size=10)

scores = model.evaluate(x_samples, y_samples)

print("\n%s: %.2f%%" % (model.metrics_names[1], scores[1]*100))

testset = np.loadtxt("test_cws.csv", delimiter=';')
x_test = dataset[:, 0:512]
y_test = to_categorical(dataset[:,512], num_classes=3)
scores = model.evaluate(x_test, y_test)

print("\n%s: %.2f%%" % (model.metrics_names[1], scores[1]*100))

x = x_samples[-8:]

model.save("model.h")

