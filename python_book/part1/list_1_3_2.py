#!/home/mbm/venv/bin/python

#chmode +x filename - to make file executabls (add such permissions)
x = list(range(10))
print(x)

text = input("enter text: ")
print("your text: ", text)

code = "print('some text from eval.input')"
eval(input())