# List your names: Richard Wang, Alexander Hails, Coleman Jenkins

import codecs
import base64
import struct

def read_int(buf, bit, length):
    total_bits_remaining = length
    val = 0
    bit_start = bit

    while total_bits_remaining > 0:
        byte = bit_start // 8
        control_bits = buf[byte] 
        local_bit_start = bit_start % 8
        bit_rem_in_byte = 8 - local_bit_start
        n_bits_filling = 0

        if local_bit_start != 0:
            n_bits_filling = min(bit_rem_in_byte, total_bits_remaining)
            keep_bits = 0
            for i in range(local_bit_start, min(local_bit_start + n_bits_filling, 8)): # removed + 1
                keep_bits |= (0x1 << (7-i))
            control_bits &= keep_bits
            # if local_bit_start > total_bits_remaining:
            if total_bits_remaining < bit_rem_in_byte:
                # print("local_bit_start:", local_bit_start)
                # print("control_bits:", control_bits)
                # print("bit_rem_in_byte:", bit_rem_in_byte)
                # print("total_bits_remaining:", total_bits_remaining)
                control_bits >>= bit_rem_in_byte - total_bits_remaining
            else:
                shift = total_bits_remaining - n_bits_filling
                control_bits <<= shift

        else: # bit_start % 8 == 0
            n_bits_filling = min(8, total_bits_remaining)
            if total_bits_remaining > n_bits_filling:
                control_bits <<= total_bits_remaining - n_bits_filling
            else:
                control_bits >>= bit_rem_in_byte - n_bits_filling

        val |= control_bits
        total_bits_remaining -= n_bits_filling
        bit_start += n_bits_filling
        

    return val

def getValue(timestamps, d):
    return timestamps[-1] + timestamps[-1] - timestamps[-2] + d

# ------ function --------
def decode(payload):
    buf = bytearray(payload)
    float_vals = []
    timestamps = []
    read_bit = 0

    empty_bits = read_int(buf, 0, 3)
    read_bit += 3

    float_vals.append(read_int(buf, 3, 32))
    read_bit += 32
    timestamps.append(read_int(buf, read_bit, 32))
    read_bit += 32

    state = "store"
    leading_bits = 0
    sig_figs = 0
    got_second = 0
    while(read_bit < len(buf)*8 - empty_bits):
        # Do the float decompression
        store_bit = read_int(buf, read_bit, 1)
        read_bit += 1
        if store_bit == 0:
            float_vals.append(float_vals[-1])
        else:
            control_bit = read_int(buf, read_bit, 1)
            read_bit += 1
            if control_bit == 1:
                leading_bits = read_int(buf, read_bit, 5)
                read_bit += 5
                sig_figs = read_int(buf, read_bit, 6)
                read_bit += 6
            # else, use previous leading_bits & sig_figs

            sig_bits = read_int(buf, read_bit, sig_figs)
            read_bit += sig_figs
            full_bits = sig_bits << (32 - sig_figs - leading_bits)
            final_value = full_bits ^ float_vals[-1]
            float_vals.append(final_value)

        # Next, find the associated timestamp
        if got_second == 0:
            v = read_int(buf, read_bit, 20)
            read_bit += 20
            timestamps.append(timestamps[-1] + v)
            got_second = 1
        else:
            found = 0
            for j in range(4):
                b = read_int(buf, read_bit, 1)
                read_bit += 1
                if b == 0:
                    if j == 0:
                        timestamps.append(getValue(timestamps, 0))
                    elif j == 1:
                        v = read_int(buf, read_bit, 7)
                        read_bit += 7
                        if v > 64:
                            v -= 128
                        timestamps.append(getValue(timestamps, v))
                    elif j == 2:
                        v = read_int(buf, read_bit, 9)
                        read_bit += 9
                        if v > 256:
                            v -= 512
                        timestamps.append(getValue(timestamps, v))
                    elif j == 3:
                        v = read_int(buf, read_bit, 12)
                        read_bit += 12
                        if v > 2048:
                            v -= 4096
                        timestamps.append(getValue(timestamps, v))
                    found = 1
                    break
            if found == 0:
                v = read_int(buf, read_bit, 32)
                read_bit += 32
                if v >= 2147483648:
                    v -= 4294967296
                timestamps.append(getValue(timestamps, v))

    converted_floats = []
    for float_int in float_vals:
        byte_rep = struct.unpack('>f', float_int.to_bytes(4, 'big'))[0]
        converted_floats.append(byte_rep)
    
    return (converted_floats, timestamps, len(buf))

