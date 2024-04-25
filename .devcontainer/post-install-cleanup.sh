#! /bin/bash

REQUIREMENTS_FILE=requirements.txt
echo '================================================='
echo "Installing $REQUIREMENTS_FILE"
if [ -f "$REQUIREMENTS_FILE" ]; then
    pip3 install -r "$REQUIREMENTS_FILE"
    rm "$REQUIREMENTS_FILE"
else
    echo "Didn't find $REQUIREMENTS_FILE, skipping..."
fi
echo '================================================='
echo "Installing all whl files"
for F in *.whl; do
    if [[ ! -f $F ]]; then
        echo "Didn't find any whl files, skipping..."
        break; 
    fi
    echo "Installing $F..."
    pip3 install "$F"
    rm "$F"
done
echo '================================================='
