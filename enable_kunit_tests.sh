    #!/bin/bash
    #
    # This script finds all Kconfig entries for KUnit tests and enables them
    # as modules ('=m') in the current .config file.
    # This is a replacement for the 'make kunitconfig' target on older kernels.
    
    set -e
    
    echo "🔍 Finding all KUnit test configurations..."
    
    # Find all Kconfig files, grep for KUnit test configs, and extract the config name.
    TEST_CONFIGS=$(grep -rh --include="*Kconfig*" "config .*[a-zA-Z0-9]_KUNIT_TEST" . | awk '{print $2}')
    
    if [ -z "$TEST_CONFIGS" ]; then
        echo "❌ No KUnit test configurations found."
        exit 1
    fi
    
    echo "✅ Found tests. Enabling them as modules in .config..."
    
    for CONFIG in $TEST_CONFIGS
    do
        echo "  -> Enabling $CONFIG"
        # Use the kernel's official config script to enable the option as a module.
        ./scripts/config --file .config --module "$CONFIG"
    done
    
    echo "🎉 All KUnit tests have been enabled."
    echo "➡️  Run 'make olddefconfig' next to apply changes before building."
    
    
