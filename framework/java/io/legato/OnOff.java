
// -------------------------------------------------------------------------------------------------
/**
 *  @file OnOff.java
 *
 *  Copyright (C) Sierra Wireless, Inc. 2017. All rights reserved.
 *
 */
// -------------------------------------------------------------------------------------------------

package io.legato;

import java.util.Map;
import java.util.HashMap;

//--------------------------------------------------------------------------------------------------
/**
 * This enum is used to represent OnOff type
 */
// --------------------------------------------------------------------------------------------------
public enum OnOff {
    OFF(0), ON(1);

    private final int value;

    OnOff(int newValue) {
        value = newValue;
    }

    public int getValue() {
        return value;
    }

    private static final Map<Integer, OnOff> valueMap = new HashMap<Integer, OnOff>();

    static {
        for (OnOff item : OnOff.values()) {
            valueMap.put(item.value, item);
        }
    }

    public static OnOff fromInt(int newValue) {
        return valueMap.get(newValue);
    }
}
