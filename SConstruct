CXX_Flags = ' -Wall -g'

env = Environment()

# Append the rest of the flags and libs
env.Append(CXXFLAGS = CXX_Flags)

# Export the environment for use elsewhere
Export('env')

SConscript(['SConscript'])
