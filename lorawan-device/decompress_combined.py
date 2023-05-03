import codecs
import base64
import struct

# Author: Richard Wang

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
            if total_bits_remaining < bit_rem_in_byte:
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
            print(f"sig_figs: {sig_figs}")
            print(f"leading_bits: {leading_bits}")
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
    
    return (converted_floats, timestamps)
        
# Encode binary data to a base 64 string
# binary_data = b'\xE8\x59\x00\x00\x1B\x42\xDA\xC3\xF7\x00'
# binary_data = b'\xc8\x41\x1c\x08\x40\x00\x00\x00\x3b\x13\xb7\xca\x20\x00\x03\xb8\xd9\x1c\xa7\x05\x8c\xcd\x7f\x95\x0b\xe0\xda\x9d\x82\x41\x56\x09\x05\x58\x24\x10\x00\x01\xb1\x4c\x9e\xc3\x59\x3d\x86\x00'
binary_data = \
b'\x08\x40\x3b\x78\x00\x00\x00\x07\xbb\x53\x90\xb8\x20\x00\x03\xb8\xd9\x04\xb6\xa7\x20\xf0\x5b\x8c\xe1\x96\x66\x7f\x92\xa1\xe3\x62\x99\x3d\x86\x03\x69\xbe\x09\x1b\x4d\xf0\x4a\xf0\x48'

# Use the codecs module to encode
base64_data = codecs.encode(binary_data, 'base64')
payload = base64.b64decode(base64_data)

print(decode(payload))