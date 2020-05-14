#!/usr/bin/env python
import numpy as np
import matplotlib.pyplot as plt

x = np.linspace(-10, 10, 1000)
y = (x - 0.5)**2
plt.plot(x, y)
plt.show()
