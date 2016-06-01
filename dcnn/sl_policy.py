import numpy as np
import chainer
from chainer import Function, Variable, optimizers, function, link
from chainer import cuda
from chainer import Link, Chain
import chainer.functions as F
import chainer.links as L
import sys
import os.path

# arg1 : cnn features

cnn_features_file = sys.argv[1]

infile = open(cnn_features_file, "rb")

# 全て読み込み
filesize = os.path.getsize(sys.argv[1])
data = infile.read(filesize)
infile.close()

BITBOARD_SIZE = int(19*19/64 + 1) * 8


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


minibatch_size = 16
feature_num = 4
k = 128
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

optimizer = optimizers.Adam()
optimizer.setup(model)
optimizer.weight_decay(0.01)

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

def forward_backward(x, t, train=True):
    z1 = F.relu(model.layer1(x))
    z2 = F.dropout(F.relu(model.layer2(z1)), train=train)
    z3 = F.dropout(F.relu(model.layer3(z2)), train=train)
    z4 = F.dropout(F.relu(model.layer4(z3)), train=train)
    z5 = F.dropout(F.relu(model.layer5(z4)), train=train)
    z6 = F.dropout(F.relu(model.layer6(z5)), train=train)
    z7 = F.dropout(F.relu(model.layer7(z6)), train=train)
    z8 = F.dropout(F.relu(model.layer8(z7)), train=train)
    z9 = F.dropout(F.relu(model.layer9(z8)), train=train)
    z10 = F.dropout(F.relu(model.layer10(z9)), train=train)
    z11 = F.dropout(F.relu(model.layer11(z10)), train=train)
    z12 = F.dropout(F.relu(model.layer12(z11)), train=train)
    u13 = model.layer13(z12)

    u13_1d = model.layer13_2(F.reshape(u13, (len(u13.data), 19*19)))

    loss = F.softmax_cross_entropy(u13_1d, t)
    print(loss.data)

    loss.backward()

DATA_SIZE = 2 + BITBOARD_SIZE * 2
data_num = len(data) / DATA_SIZE
pos = 0
for i in range(2000):
    features_data = []
    teacher_data = []
    for j in range(minibatch_size):
        # ランダムに選択
        pos = np.random.randint(0, data_num) * DATA_SIZE

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
        #print(features_data[j])
        teacher_data.append(xy)

    # 入力
    features_nparray = np.array(features_data, dtype=np.float32)
    features = Variable(cuda.to_gpu(features_nparray))

    # 教師データ
    teacher = Variable(cuda.to_gpu(np.array(teacher_data)))

    optimizer.zero_grads()
    forward_backward(features, teacher)
    optimizer.update()
