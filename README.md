# sisl-vp

Enhanced version of [SymEx-VP](https://github.com/agra-uni-bremen/symex-vp) with support for [SISL](https://github.com/agra-uni-bremen/sisl) input specifications.

## Installation

This software is best installed using [Docker](https://www.docker.io/).
To build a Docker image for SISL-VP run the following command:

	$ docker build -t sisl-vp .

Afterwards, create a new Docker container from this image using:

	$ docker run -it sisl-vp

Within the Docker container both the [SISL Scheme
DSL](https://github.com/agra-uni-bremen/sisl) as well as the SISL
enhanced version of SymEx-VP are available.

## Acknowledgements

This work was supported in part by the German Federal Ministry of
Education and Research (BMBF) within the project Scale4Edge under
contract no. 16ME0127 and within the project VerSys under contract
no. 01IW19001.

## License

The original riscv-vp code is licensed under MIT (see `LICENSE.MIT`).
All modifications made for the integration of symbolic execution with
riscv-vp are licensed under GPLv3+ (see `LICENSE.GPL`). Consult the
copyright headers of individual files for more information.
