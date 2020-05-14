#!/usr/bin/env python
print("hellow world")


L = [1,2]
L.append(L)
print(L)

l = list(range(10))
print(l)
l2 = list(range(1,10))
print(l2)

def m_foo(a, b, size):
	x = 0.
	y = 0.
	for i in range(0, size+1):
		x += a + i
		y += b + i
	return x/y