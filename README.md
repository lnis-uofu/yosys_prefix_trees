# Adder synthesis Yosys plugin

## Table of Contentso

[Introduction](#introduction)

[How to install](#installation-instructions)

[How to use](#user-guide)

## Introduction

An open-source hardware adder generation library is under active development
[here](https://github.com/tdene/synth_opt_adders/).  This repository aims to
provide a plugin that will allow Yosys to synthesize adders using this library.

## Installation instructions

In order to use this repository, the following programs are required:

 * Python 3.6+ (`apt install python3`)
 * A C compiler?

Begin by cloning this repository, either via HTTPS:

```
git clone https://github.com/lnis-uofu/yosys_prefix_trees.git
```

or via SSH:

```
git clone git@github.com:lnis-uofu/yosys_prefix_trees.git
```

Next, install the pptrees library as a local Python package:

```
pip3 install git+https://github.com/tdene/synth_opt_adders.git --user
```

Install Yosys, either by building from
[source](https://github.com/YosysHQ/yosys), or via package manager:

```
apt install yosys
```

Install `yosys-dev` via package manager:

```
apt install yosys-dev
```

Finally, build the C code-base of this repository:

```
cd yosys_prefix_trees
make build
```

## User guide

TBD
