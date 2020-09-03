#!/bin/bash
rm -R CHARMing
git clone https://github.com/zweistein-frm2/CHARMing
cd CHARMing && git submodule init && git submodule update
cd ..


