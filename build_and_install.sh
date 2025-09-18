#!/bin/bash

# Salis Kuwahara Filter Build and Install Script

echo "Building Salis Kuwahara Filter..."

# Build the project
cd Xcode
xcodebuild -project SalisKuwaharaFilter.xcodeproj -scheme SalisKuwaharaFilter -configuration Release build

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo "Build succeeded!"
    
    # Create installation directory if needed
    INSTALL_DIR="/Applications/Adobe After Effects 2025/Plug-ins/Salis Effects"
    
    echo "Installing to: $INSTALL_DIR"
    
    # Create directory if it doesn't exist
    sudo mkdir -p "$INSTALL_DIR"
    
    # Copy the plugin
    sudo cp -R build/Release/SalisKuwaharaFilter.plugin "$INSTALL_DIR/"
    
    if [ $? -eq 0 ]; then
        echo "Installation complete!"
        echo "Please restart After Effects to use the Salis Kuwahara Filter."
    else
        echo "Installation failed. Please check permissions."
    fi
else
    echo "Build failed. Please check the error messages above."
fi