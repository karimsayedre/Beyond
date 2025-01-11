import os
import subprocess
import CheckPython

# Make sure everything we need is installed
CheckPython.ValidatePackages()

import Utils
import colorama

from colorama import Fore
from colorama import Back
from colorama import Style

colorama.init()

# Change from Scripts directory to root
os.chdir('../')

print(f"{Style.BRIGHT}{Back.GREEN}Generating Visual Studio 2022 solution.{Style.RESET_ALL}")
subprocess.call(["vendor/bin/premake5.exe", "vs2022", "--verbose"])
os.chdir('Editor/SandboxProject')
subprocess.call(["../../vendor/bin/premake5.exe", "vs2022", "--verbose"])
