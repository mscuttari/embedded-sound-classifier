#autoencoder


from keras.models import Sequential
from keras.optimizers import RMSprop
from keras.layers import Dense

import numpy as np

batch_size = 128
epochs = 50
inChannel = 1

dataset = np.loadtxt("10space_pause_10space_pause.pcm_FFT.csv", delimiter=';')
dataset_s = np.loadtxt("silence.pcm_FFT.csv", delimiter=';')

dataset = np.concatenate((dataset, dataset_s), axis=0)
np.random.shuffle(dataset)

x_samples = dataset[:, 0:254]
y_samples = dataset[:, 254]

model = Sequential()
model.add(Dense(254, input_dim=254, activation='relu'))
model.add(Dense(18, activation='relu'))
model.add(Dense(1, activation='sigmoid'))

model.compile(loss='binary_crossentropy', optimizer = 'adam', metrics=['accuracy'])
model.fit(x_samples, y_samples, epochs=100, batch_size=10)
print(model.metrics_names)

scores = model.evaluate(x_samples, y_samples)

for i in range(len(scores)) :
    print("%s: %.2f%%" % (model.metrics_names[i],(scores[i]*100)))
