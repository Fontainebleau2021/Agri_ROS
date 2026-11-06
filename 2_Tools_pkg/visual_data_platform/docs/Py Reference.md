### Python 模块格式说明

简要的模板如下

```python
from NodeBase import NodeBase

class className(NodeBase):
    def test(self, *args, **kwargs):
        return []

    def train(self, *args, **kwargs):
        return []
```

每个模块需要继承NodeBase模块，可以是间接继承， 为了了解Python逻辑，我们也看一下这个基类

```js
import json


class NodeBase():
    def __init__(self):
        super().__init__()

    def preload(self, **kwargs):
        """模型预加载, 按照存储的信息导入
        """
        pass

    def train(self, *args, **kwargs):
        """训练函数
        """
        pass

    def test(self, *args, **kwargs):
        """运行函数
        """
        pass

    def reportProgress(self, value):
        """汇报进度
        """
        msg = json.dumps({
            "nodeId": self.id,
            "value": value
        })
        print(f'$node-progress$:{msg}', flush=True)

    def run(self, *args, **kwargs):
        self.id = kwargs["nodeId"]
        self.reportProgress(0)
        if kwargs['mode'] == 'train':
            result = self.train(*args, **kwargs)
            self.reportProgress(100)
            return result
        else:
            result = self.test(*args, **kwargs)
            self.reportProgress(100)
            return result
```



执行逻辑

- 训练模式下

  直接执行`train`函数

- 应用模式下

  ==首先执行`preload`函数预加载==，然后执行`test`函数



任何子类都可以调用`reportProgress` 反映进度，通常模块不需要，当执行时间超过10秒的时候建议使用



#### 学习一： 输入参数意义

```
*args 表示模块的输入
    args是一个列表，前端通过连线的方式连接不同模块的输入和输出，args就是上游模块连线到当前模块的输入
    args的长度等于模块输入锚点的个数
    args的输入顺序对应输入锚点的顺序
    当输入锚点没有连线，对应的args为None
    获取第i个数据使用args[i], i从0开始

**kwargs 表示前端模块的配置数据
	kwargs是一个字典
	其中模块可以配置哪些参数和默认值是通过模块对应的js进行配置的
	假设配置了参数x, 前端设置了参数，那么可以通过kwargs["x"]获取前端配置的结果
```



#### 学习二：输出参数的配置

参数的输出是一个列表，如果没有输出，应该 `return []`

- 输出列表和输出的锚点是对应的
- 第i个输出锚点对应输出列表里面的第i个输出
- 输出列表长度可以大于输出锚点的个数，但是这部分输出无法被其余模块作为输入(因为没法连线)，但是这个规则存在的意义在于可以多输出一些数据保存起来用于模块结果的可视化。



#### 学习三： 时序数据参数格式

时序数据参数类似一个表格，大致数据格式如下

```
日期	最低温度	最高温度
2020/7/1	22	29
2020/7/2	22	27
2020/7/3	22	30
2020/7/4	24	27
2020/7/5	25	26
2020/7/6	25	27
2020/7/7	24	33
2020/7/8	24	30
2020/7/9	24	27
```

第一个维度表示时刻，第二个维度表示特征

为了把时间和标题同时保存起来，所以推荐使用 `pandas.DataFrame`, 最好不同模块之间传递都遵循此规则，省去数据格式混乱



#### 学习四： 模型的保存和预加载

某些模块涉及到训练和应用的过程，在训练的时候需要将模型保存起来，在应用的时候需要将模型加载进来

`STEP1` 

模块增加配置参数，用户可以设置模型保存的名称 `FileName` 

`STEP2`

约定模型保存的目录为 `STORE_PATH = kwargs["STORE_PATH"]` 

拼合Path得到实际保存的路径 `STORE_PATH + FileName` 

不同模块还可以在`STORE_PATH ` 下建立各自的文件夹，约定好文件夹的名称，那么路径变更为

 `STORE_PATH + 模块约定的文件夹 + FileName` 

`STEP3`

按照模型的类别选择合适的序列化方式，保存数据到对应的路径

`STEP4` 

在预加载函数里面按照保存的路径反向将模型读取出来，保存为类的成员变量，然后在test函数中就可以应用



#### 学习五： 高级用法

如果你的很多模块都使用了相同的外壳，可以先建立一个标准类

```python
from NodeBase import NodeBase

class StandardBase(NodeBase):
	def __init__(self, specialAOP):
        self.__specialAOP = specialAOP
        super(nnModule, self).__init__()
        
    def test(self, *args, **kwargs):
    	# use specialAOP
        return []

    def train(self, *args, **kwargs):
    	# use specialAOP
        return []
```

然后定义模块的时候可以采用如下方式：

```python
def module1():
    return StandardBase(module1SpecialClass)

def module2():
    return StandardBase(module2SpecialClass)
```

典型场景如深度学习框架，训练和测试过程都是基本一致的，只有模型发生了主要的变化，那么不同的模型定义就是一个需要注入的元素。



#### 学习六：打印信息

在Python文件中直接使用print语句可以在前端console中看到输出

```python
print('content', flush=True)
```



