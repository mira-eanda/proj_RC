#!/bin/bash

for SCRIPT_NUMBER in {1..29}; do
    echo "Executing script $SCRIPT_NUMBER..."
    echo "193.136.128.108 58033 $SCRIPT_NUMBER" | nc tejo.tecnico.ulisboa.pt 59000 >> report$SCRIPT_NUMBER.html
done

echo "All scripts executed."