import sys

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

# arg1 : cnn features

cnn_features_file = sys.argv[1]

infile = open(cnn_features_file, "rb")

for i in range(10):
    xy = int.from_bytes(infile.read(2), 'little')
    player_color_bitboard = infile.read(int(19*19/64 + 1) * 8)
    opponent_color_bitboard = infile.read(int(19*19/64 + 1) * 8)


    print(xy, xy % 19 + 1, int(xy / 19) + 1)
    player_color = bitboard_to_array(player_color_bitboard)
    print(player_color)
    opponent_color = bitboard_to_array(opponent_color_bitboard)
    print(opponent_color)

infile.close()
