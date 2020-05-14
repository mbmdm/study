from imp import reload

print('\n loading modul:')
import list_1_3_3
	
print('\n using function reload():')
reload(list_1_3_3)

print("print modul.attribute:")
print(list_1_3_3.title)


print("\n using function dir():")
print(dir(list_1_3_3))

print("\nload modul using function exec() :")
dexec(open('list_1_3_3.py').read())
