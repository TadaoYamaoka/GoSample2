import numpy as np
import chainer
from chainer import Function, Variable, optimizers, function, link, serializers
from chainer import cuda
from chainer import Link, Chain
import chainer.functions as F
import chainer.links as L
import os.path
import argparse

# 定数
BITBOARD_SIZE = int(19*19/64 + 1) * 8
DATA_SIZE = 2 + BITBOARD_SIZE * 2

# 起動パラメータ(e.g. ..\learn\cnn_features.bin sl_policy.model)
parser = argparse.ArgumentParser(description='SL policy predict')
parser.add_argument('cnn_feature_input_file',
                    help='features input file')
parser.add_argument('model_file',
                    help='sl policy model file')
args = parser.parse_args()

# 全て読み込み
infile = open(args.cnn_feature_input_file, "rb")
filesize = os.path.getsize(args.cnn_feature_input_file)
data = infile.read(filesize)
infile.close()
data_num = int(len(data) / DATA_SIZE)

# bias
def _as_mat(x):
    if x.ndim == 2:
        return x
    return x.reshape(len(x), -1)

class MyBiasFunction(function.Function):

    def forward(self, inputs):
        x = _as_mat(inputs[0])
        b = inputs[1]
        y = x + b
        return y,

    def backward(self, inputs, grad_outputs):
        x = _as_mat(inputs[0])
        gy = grad_outputs[0]

        gx = gy.reshape(inputs[0].shape)
        gb = gy.sum(0)
        return gx, gb

class MyBias(link.Link):

    def __init__(self, size, bias=0, initial_bias=None):
        super(MyBias, self).__init__()
        self.add_param('b', size)
        if initial_bias is None:
            initial_bias = bias
        self.b.data[...] = initial_bias

    def __call__(self, x):
        return MyBiasFunction()(x, self.b)

# DCNN定数
minibatch_size = 2
feature_num = 4
k = 192

model = Chain(
    layer1=L.Convolution2D(in_channels = feature_num, out_channels = k, ksize = 5, pad = 2),
    layer2=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer3=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer4=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer5=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer6=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer7=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer8=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer9=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer10=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer11=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer12=L.Convolution2D(in_channels = k, out_channels = k, ksize = 3, pad = 1),
    layer13=L.Convolution2D(in_channels = k, out_channels = 1, ksize = 1, nobias = True),
    layer13_2=MyBias(19*19))

model.to_gpu()

serializers.load_npz(args.model_file, model)

#print(model.layer1.W.data) # debug
#print(model.layer1.b.data) # debug

def bitboard_to_array(bitboard):
    a = []
    xy = 0
    for b in bitboard:
        for i in range(8):
            x = xy % 19
            y = int(xy / 19)
            c = (b >> i) & 1
            if x == 0:
                a.append([c])
            else:
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

def forward(x):
    z1 = F.relu(model.layer1(x))
    #print(z1.data) # debug
    z2 = F.relu(model.layer2(z1))
    z3 = F.relu(model.layer3(z2))
    z4 = F.relu(model.layer4(z3))
    z5 = F.relu(model.layer5(z4))
    z6 = F.relu(model.layer6(z5))
    z7 = F.relu(model.layer7(z6))
    z8 = F.relu(model.layer8(z7))
    z9 = F.relu(model.layer9(z8))
    z10 = F.relu(model.layer10(z9))
    z11 = F.relu(model.layer11(z10))
    z12 = F.relu(model.layer12(z11))
    u13 = model.layer13(z12)

    u13_1d = model.layer13_2(F.reshape(u13, (len(u13.data), 19*19)))

    return u13_1d

def set_minibatch(data, data_num, i = 0):
    features_data = []
    teacher_data = []
    for j in range(minibatch_size):
        pos = i * DATA_SIZE

        xy = int.from_bytes(data[pos : pos + 2], 'little')
        #print("xy=", xy)
        pos += 2
        player_color_bitboard = data[pos : pos + BITBOARD_SIZE]
        pos += BITBOARD_SIZE
        opponent_color_bitboard = data[pos : pos + BITBOARD_SIZE]
        pos += BITBOARD_SIZE

        player_color = bitboard_to_array(player_color_bitboard)
        opponent_color = bitboard_to_array(opponent_color_bitboard)
        empty = make_empty_array(player_color, opponent_color)

        features_data.append([player_color, opponent_color, empty, [[1]*19]*19])
        teacher_data.append(xy)

        # check
        if xy < 0 or xy >= 19*19:
            raise NameError('xy incorrect')
        if empty[int(xy/19)][xy%19] != 1:
            print(player_color)
            print(opponent_color)
            print(empty)
            raise NameError('empty incorrect pos={}, xy={}, x={}, y={}'.format(pos, xy, xy%19, int(xy/19)))
        sum_c = 0
        for c in player_color:
            sum_c += sum(c)
        for c in opponent_color:
            sum_c += sum(c)
        for c in empty:
            sum_c += sum(c)
        if sum_c != 19*19:
            raise NameError('board incorrect')

    # 入力
    #features = Variable(np.array(features_data, dtype=np.float32))
    features = Variable(cuda.to_gpu(np.array(features_data, dtype=np.float32)))

    # 教師データ
    #teacher = Variable(np.array(teacher_data))
    teacher = Variable(cuda.to_gpu(np.array(teacher_data)))

    return features, teacher

# ミニバッチデータ
features, teacher = set_minibatch(data, data_num, 0)

# 順伝播
u13_1d = forward(features)

y_1d = F.softmax(u13_1d)
y = F.reshape(y_1d, (minibatch_size, 19, 19))

print(y.data)
