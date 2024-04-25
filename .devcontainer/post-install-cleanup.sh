#! /bin/bash

REQUIREMENTS_FILE=requirements.txt
echo "Installing $REQUIREMENTS_FILE"
if [ -f "$REQUIREMENTS_FILE" ]; then
    pip3 install -r "$REQUIREMENTS_FILE"
    rm "$REQUIREMENTS_FILE"
fi

WHEEL_FILES='*.whl'
echo "Installing $WHEEL_FILES"
if [ -f "$WHEEL_FILES" ]; then
    pip3 install "$WHEEL_FILES"
    rm "$WHEEL_FILES"
fi
