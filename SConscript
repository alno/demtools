Import('env')

Required_Libs = Split('''gdal''')

local_env = env.Copy()
local_env.Append(LIBS = Required_Libs)

cxxlist = Split('''color-relief.cxx''')

local_env.Program(target = 'color-relief', source = cxxlist)
