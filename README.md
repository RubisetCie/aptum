APT
===

`apt` is the main command-line package manager for *Debian* and its derivatives.
It provides command-line tools for searching and managing as well as querying information about packages and low-level access to all features provided by the `libapt-pkg` and `libapt-inst` libraries which higher-level package managers can depend upon.

Included tools are:

- **apt-get** for retrieval of packages and information about them
- **apt-cache** for querying available information about installed as well as available packages.
- **apt-cdrom** to use removable media as a source for packages.
- **apt-config** as an interface to the configuration settings.
- **apt-key** as an interface to manage authentication keys.
- **apt-extracttemplates** to be used by debconf to prompt for configuration questions before installation.
- **apt-ftparchive** creates Packages and other index files needed to publish an archive of *deb* packages.
- **apt-sortpkgs** is a Packages/Sources file normalizer.
- **apt** is a high-level command-line interface for better interactive usage.


APTUM
-----

APTUM expands APT with the following features:

- Temporary state support (save state / loading):
	- `apt save`: initialize a temporary state, for further loading
	- `apt load`: load a previously saved temporary state
- Shortcut to display close history:
	- `apt history`: print the history of commands:
	- `apt history -f`: print the full history
- Option `-architecture=<arch>` to select architecture for all installing packages
- Always asking for confirmation before installing a package


Building
--------
First, you need to install some dependency packages:

```
# Building tools
build-essential
pkg-config
triehash
cmake

# Dependencies
gnutls-dev
libdb-dev
libudev-dev
libbz2-dev
libzstd-dev
libsystemd-dev
libseccomp-dev
libssl-dev
libgcrypt20-dev
libxxhash-dev
liblzma-dev
liblz4-dev
zlib1g-dev

# Native-language support
gettext

# Documentation building
doxygen
docbook-xml
docbook-xsl
xsltproc
po4a
w3m

# Tests running
libgtest-dev
```

Then to start the configuration, you need to run the following from a build directory:

	cmake <path to source directory>

Example of standard build:

	cmake -DCMAKE_BUILD_TYPE=Release -DWITH_DOC=ON -DWITH_TESTS=ON .

Example of build with keeping the system state:

	cmake -DCMAKE_BUILD_TYPE=Release -DSTATE_DIR=/var/lib/apt -DLOG_DIR=/var/log/apt -DCACHE_DIR=/var/cache/apt -DCONF_DIR=/etc/apt -DLIBEXEC_DIR=/usr/lib/apt .

Testing
-------
There is an extensive integration test suite available which can be run via:

	./test/integration/run-tests


Additional documentation
------------------------

Many more things could and should be said about APT and its usage but are more targeted at developers of related programs or only of special interest.

- [Protocol specification of APT's communication with external dependency solvers (EDSP)](./doc/external-dependency-solver-protocol.md)
- [Protocol specification of APT's communication with external installation planners (EIPP)](./doc/external-installation-planner-protocol.md)
- [How to use and configure APT to acquire additional files in 'update' operations](./doc/acquire-additional-files.md)
- [Download and package installation progress reporting details](./doc/progress-reporting.md)
- [Remarks on DNS SRV record support in APT](./doc/srv-records-support.md)
- [Protocol specification of APT interfacing with external hooks via JSON](./doc/json-hooks-protocol.md)
