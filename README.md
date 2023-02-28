APT
===

`apt` is the main command-line package manager for *Debian* and its derivatives.
It provides command-line tools for searching and managing as well as querying information about packages and low-level access to all features provided by the `libapt-pkg` and `libapt-inst` libraries which higher-level package managers can depend upon.

Included tools are:

- **apt-get** for retrieval of packages and information about them from authenticated sources and for installation, upgrade and removal of packages together with their dependencies.
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

- Rollback


Building
--------
To start the configuration, you need to run the following from a build directory:

	cmake <path to source directory>


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
