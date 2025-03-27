#!/bin/bash

cd scripts
python3 genConfigFromBase.py
python3 tempToMurphiSrc.py ../src/template/RRC.m all
python3 runMurphi.py all