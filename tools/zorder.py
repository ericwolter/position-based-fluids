# import struct
# from struct import pack, unpack
# from math import frexp

# dim = 2



# print pos

# def xor_float(f1, f2):
#     f1 = int(''.join(hex(ord(e))[2:] for e in struct.pack('d',f1)),16)
#     f2 = int(''.join(hex(ord(e))[2:] for e in struct.pack('d',f2)),16)
#     xor = f1 ^ f2
#     xor = "{:016x}".format(xor)
#     xor = ''.join(chr(int(xor[i:i+2],16)) for i in range(0,len(xor),2))
#     return struct.unpack('d',xor)[0]

# def less_msb(a,b):
#     (m_a, e_a) = frexp(a)
#     (m_b, e_b) = frexp(b)
#     if e_a != e_b:
#         return e_a < e_b
#     else:
#         return m_a < m_b and m_a < (xor_float(m_a,m_b))

# def cmp_zorder(a,b):
#     j = 0
#     k = 0
#     x = 0
#     for k in range(dim):
#         y = xor_float(a[k], b[k])
#         if less_msb(x,y):
#             j = k
#             x = y
#     if a[j] > b[j]:
#         return 1
#     elif a[j] < b[j]:
#         return -1
#     else:
#         return 0

# def less_msb_int(a,b):
#     return a < b and a < (a ^ b)        

# def cmp_zorder_int(a,b):
#     j = 0
#     k = 0
#     x = 0
#     for k in range(dim):
#         y = a[k] ^ b[k]
#         if less_msb_int(x,y):
#             j = k
#             x = y
#     if a[j] > b[j]:
#         return 1
#     elif a[j] < b[j]:
#         return -1
#     else:
#         return 0


# pos_sorted = sorted(pos, cmp=cmp_zorder_int)

# print pos_sorted

def part1by1(n):
        n&= 0x0000ffff
        n = (n | (n << 8)) & 0x00FF00FF
        n = (n | (n << 4)) & 0x0F0F0F0F
        n = (n | (n << 2)) & 0x33333333
        n = (n | (n << 1)) & 0x55555555
        return n


def unpart1by1(n):
        n&= 0x55555555
        n = (n ^ (n >> 1)) & 0x33333333
        n = (n ^ (n >> 2)) & 0x0f0f0f0f
        n = (n ^ (n >> 4)) & 0x00ff00ff
        n = (n ^ (n >> 8)) & 0x0000ffff
        return n


def interleave2(x, y):
        return part1by1(x) | (part1by1(y) << 1)


def deinterleave2(n):
        return unpart1by1(n), unpart1by1(n >> 1)

pos = [
    (0,0), (1,0), (2,0), (3,0), (4,0), (5,0), (6,0), (7,0), 
    (0,1), (1,1), (2,1), (3,1), (4,1), (5,1), (6,1), (7,1), 
    (0,2), (1,2), (2,2), (3,2), (4,2), (5,2), (6,2), (7,2), 
    (0,3), (1,3), (2,3), (3,3), (4,3), (5,3), (6,3), (7,3), 
    (0,4), (1,4), (2,4), (3,4), (4,4), (5,4), (6,4), (7,4), 
    (0,5), (1,5), (2,5), (3,5), (4,5), (5,5), (6,5), (7,5), 
    (0,6), (1,6), (2,6), (3,6), (4,6), (5,6), (6,6), (7,6), 
    (0,7), (1,7), (2,7), (3,7), (4,7), (5,7), (6,7), (7,7), 
]

pos = [
    (0.0,0.0), (0.1,0.0), (0.2,0.0), (0.3,0.0), (0.4,0.0), (0.5,0.0), (0.6,0.0), (0.7,0.0), 
    (0.0,0.1), (0.1,0.1), (0.2,0.1), (0.3,0.1), (0.4,0.1), (0.5,0.1), (0.6,0.1), (0.7,0.1), 
    (0.0,0.2), (0.1,0.2), (0.2,0.2), (0.3,0.2), (0.4,0.2), (0.5,0.2), (0.6,0.2), (0.7,0.2), 
    (0.0,0.3), (0.1,0.3), (0.2,0.3), (0.3,0.3), (0.4,0.3), (0.5,0.3), (0.6,0.3), (0.7,0.3), 
    (0.0,0.4), (0.1,0.4), (0.2,0.4), (0.3,0.4), (0.4,0.4), (0.5,0.4), (0.6,0.4), (0.7,0.4), 
    (0.0,0.5), (0.1,0.5), (0.2,0.5), (0.3,0.5), (0.4,0.5), (0.5,0.5), (0.6,0.5), (0.7,0.5), 
    (0.0,0.6), (0.1,0.6), (0.2,0.6), (0.3,0.6), (0.4,0.6), (0.5,0.6), (0.6,0.6), (0.7,0.6), 
    (0.0,0.7), (0.1,0.7), (0.2,0.7), (0.3,0.7), (0.4,0.7), (0.5,0.7), (0.6,0.7), (0.7,0.7), 
]

def cmp_interleave(a,b):
    return interleave2(int(a[0]),int(a[1])) - interleave2(int(b[0]),int(b[1]))

def to_int(a):
    return (int(a[0]*100),int(a[1]*100))

pos = [to_int(p) for p in pos]
print pos

pos_sorted = sorted(pos, cmp=cmp_interleave)

print pos_sorted
