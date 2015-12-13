#!/usr/bin/env python

# http://docs.scipy.org/doc/numpy/reference/routines.fft.html

import numpy
import matplotlib.pyplot as plt
import time
import random


def read_signal(fsampl, tsampl, x = None, noise = False):

    nsampl = 1 + int(fsampl / tsampl)
    fsin = 20.0

    if x == None: x = numpy.empty(nsampl)

    for i in range(0, nsampl):
        x[i] = numpy.sin(2.0 * numpy.pi * fsin * float(i) / fsampl)

    if noise == True:
        random.seed()
        for i in range(0, nsampl):
            r = random.randint(0, 10000)
            x[i] += float(r) / 1000000.0

    return x


def compute_power_spectrum(x, y = None):

    c = numpy.fft.rfft(x, norm = 'ortho')
    # c.size = x.size / 2 + 1

    if y == None: y = numpy.empty(c.size)

    q = 0.0
    for i in range(0, c.size):
        y[i] = numpy.sqrt(numpy.real(c[i]) ** 2 + numpy.imag(c[i]) ** 2)
        q += y[i]

    for i in range(0, c.size):
        y[i] = y[i] / q

    return y


def plot_power_spectrum(y, fsampl):
    if plot_power_spectrum.fig == None:
        plot_power_spectrum.fig = plt.figure()
        plt.xlabel('freq (Hz)')
        plt.ylabel('magnitude (%)')
        plt.show(block = False)

    fres = fsampl / float(y.size * 2)
    x = numpy.empty(y.size)
    for i in range(0, x.size): x[i] = float(i) * fres

    plt.clf()
    plt.ylim((0,1))
    plt.plot(x, y)
    plt.draw()


def main():
    plot_power_spectrum.fig = None

    fsampl = 1000.0
    tsampl = 1.0

    while True:
        x = read_signal(fsampl, tsampl, noise = True)
        y = compute_power_spectrum(x)

        # fres = fsampl / float(y.size * 2)
        # for i in range(0, y.size):
        # print(str(float(i) * fres) + ' ' + str(y[i] / q))

        plot_power_spectrum(y, fsampl)

        time.sleep(tsampl / 4.0)

main()
