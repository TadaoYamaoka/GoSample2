import numpy as np
import chainer
from chainer import Function, Variable, optimizers, function, link, serializers
from chainer import cuda
from chainer import Link, Chain
import chainer.functions as F
import chainer.links as L
import os.path
import argparse
import time
import random

# 定数
BITBOARD_SIZE = int(19*19/64 + 1) * 8
DATA_SIZE = 2 + BITBOARD_SIZE * 2

# 起動パラメータ
parser = argparse.ArgumentParser(description='SL policy learning')
parser.add_argument('cnn_feature_input_file',
                    help='cnn features input file')
parser.add_argument('--test_file', '-t', default='',
                    help='test file')
parser.add_argument('--initmodel', '-m', default='',
                    help='Initialize the model from given file')
parser.add_argument('--resume', '-r', default='',
                    help='Resume the optimization from snapshot')
parser.add_argument('--data_range', '-d', default='',
                    help='data range to use from input file')
parser.add_argument('--iteration', '-i', default='',
                    help='iteration time')
parser.add_argument('--weight_decay', '-w', default='0.003',
                    help='weight decay')
parser.add_argument('--no_save', action='store_true',
                    help='no save')
parser.add_argument('--test_mode', action='store_true',
                    help='predict only')
args = parser.parse_args()

# 全て読み込み
infile = open(args.cnn_feature_input_file, "rb")
filesize = os.path.getsize(args.cnn_feature_input_file)
data = infile.read(filesize)
infile.close()
data_num = int(len(data) / DATA_SIZE)
if args.data_range != '':
    data_num = int(args.data_range)

# シャッフル
poslist = list(range(data_num))
random.shuffle(poslist)

# テストデータ読み込み
if args.test_file != '':
    testfile = open(args.test_file, "rb")
    testfilesize = os.path.getsize(args.test_file)
    testdata = testfile.read(testfilesize)
    testfile.close()
    testdata_num = len(testdata) / DATA_SIZE

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
minibatch_size = 16
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

optimizer = optimizers.SGD()
optimizer.setup(model)
optimizer.weight_decay(float(args.weight_decay))

# Init/Resume
if args.initmodel:
    print('Load model from', args.initmodel)
    serializers.load_npz(args.initmodel, model)
if args.resume:
    print('Load optimizer state from', args.resume)
    serializers.load_npz(args.resume, optimizer)


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

def set_minibatch(data, data_num, poslist = None, i = -1):
    features_data = []
    teacher_data = []
    for j in range(minibatch_size):
        if poslist == None:
            # ランダムに選択
            pos = np.random.randint(0, data_num) * DATA_SIZE
        else:
            pos = poslist[i + j] * DATA_SIZE

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

if args.iteration != '':
    iteration = int(args.iteration)
else:
    iteration = int(data_num / minibatch_size)

N = 100
N_test = 10
sum_loss = 0
sum_acc = 0
start = time.time()
start_total = start
for i in range(iteration):
    # ミニバッチデータ
    features, teacher = set_minibatch(data, data_num, poslist, i * minibatch_size)

    # 順伝播
    u13_1d = forward(features)

    if args.test_mode == False:
        optimizer.zero_grads()
        loss = F.softmax_cross_entropy(u13_1d, teacher)
        #print(loss.data)
        sum_loss += loss.data
        loss.backward()
        optimizer.update()

        # lossとacc表示
        if (i + 1) % N == 0:
            end = time.time()
            elapsed_time = end - start
            throughput = N / elapsed_time
            print('train mean loss={}, throughput={} minibatch/sec'.format(sum_loss / N, throughput), end='')

            # accuracy
            if args.test_file != '':
                sum_acc = 0
                for i_test in range(N_test):
                    x_test, t_test = set_minibatch(testdata, testdata_num)
                    # 順伝播
                    u13_1d = forward(x_test)
                    sum_acc += F.accuracy(u13_1d, t_test).data

                print(', accuracy={}'.format(sum_acc / N_test), end='')

            print()
            start = time.time()
            sum_loss = 0
    else:
        # test mode
        sum_acc += F.accuracy(u13_1d, teacher).data

end_total = time.time()
elapsed_total_time = end_total - start_total
throughput_total = iteration / elapsed_total_time
print('elapsed total time = {} sec, throughput = {} minibatch/sec'.format(elapsed_total_time, throughput_total))

if args.test_mode == True:
    print(' accuracy={}'.format(sum_acc / iteration))

# Save the model and the optimizer
if args.no_save == False and args.test_mode == False:
    print('save the model')
    serializers.save_npz('sl_policy.model', model)
    print('save the optimizer')
    serializers.save_npz('sl_policy.state', optimizer)
