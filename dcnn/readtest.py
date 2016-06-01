import sys
import os.path

def bitboard_to_array(bitboard):
    a = []
    xy = 0
    for b in bitboard:
        for i in range(8):
            x = xy % 19
            y = int(xy / 19)
            if x == 0:
                a.append([])
            c = (b >> i) & 1
            a[y].append(c)
            xy += 1
            if xy == 19*19:
                return a

def make_empty_array(player_color, opponent_color):
    empty = []
    for y in range(19):
        empty.append([])
        for x in range(19):
            empty[y].append((player_color[y][x] | opponent_color[y][x]) ^ 1)
    return empty

# arg1 : cnn features

cnn_features_file = sys.argv[1]

infile = open(cnn_features_file, "rb")

# 全て読み込み
filesize = os.path.getsize(sys.argv[1])
data = infile.read(filesize)
infile.close()

BITBOARD_SIZE = int(19*19/64 + 1) * 8

pos = 0
for i in range(10):
    xy = int.from_bytes(data[pos : pos + 2], 'little')
    pos += 2
    player_color_bitboard = data[pos : pos + BITBOARD_SIZE]
    pos += BITBOARD_SIZE
    opponent_color_bitboard = data[pos : pos + BITBOARD_SIZE]
    pos += BITBOARD_SIZE

    print(xy, xy % 19 + 1, int(xy / 19) + 1)
    player_color = bitboard_to_array(player_color_bitboard)
    print(player_color)
    opponent_color = bitboard_to_array(opponent_color_bitboard)
    print(opponent_color)
    empty = make_empty_array(player_color, opponent_color)
    print(empty)

