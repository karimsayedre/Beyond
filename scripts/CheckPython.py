import subprocess

def InstallPackage(package):
    print(f"Installing {package} module...")
    subprocess.check_call(['python', '-m', 'pip', 'install', package])

# Mandatory
# NOTE: pkg_resources is deprecated as an API. See https://setuptools.pypa.io/en/latest/pkg_resources.html
InstallPackage('setuptools')

import pkg_resources

from importlib.metadata import distributions, distribution
from importlib.metadata import PackageNotFoundError
import subprocess
import sys

def ValidatePackage(package_name):
    try:
        # This will raise PackageNotFoundError if package isn't found
        distribution(package_name)
    except PackageNotFoundError:
        InstallPackage(package_name)


def ValidatePackages():
    ValidatePackage('requests')
    ValidatePackage('fake-useragent')
    ValidatePackage('colorama')