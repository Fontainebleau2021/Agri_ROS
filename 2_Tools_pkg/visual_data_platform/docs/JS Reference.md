### Python 模块JS配置格式说明

案例： 参考CSV的配置

#### 学习一：配置基本属性

- 展示的图标 icon，==采用图片==

- 是否在菜单中显示 visible ==布尔量==

- 展示的名称 label

- 画布中节点的类型 type， ==新类型node-chip==

  需要预先定义好特定的类型，目前给定的只有`node-base` , 左边是输入锚点，右边是输出锚点，呈现形式为方块状，顶部有标题栏展示label

- 节点的宽度 width ==新的类型node-chip建议60==

  单位是像素

- 节点的高度 height ==新的类型node-chip建议60==

  单位是像素

- Python模块引入的路径 modulePath

  应该是相对model文件夹的路径: package名.path1.path2.module名

- Python模块引入的类或者函数 moduleKey

  对应引入路径中创建的类，一般采用与module同名

- python文件路径，用于前端打开文件编辑 pythonPath

  需要引入path模块，路径为`path.join(__dirname, python文件名)` 

- 配置文件路径 configPath

  如果采用index.js命名，那么始终为 `path.join(__dirname, 'index.js')`

- 帮助文档 helpDocument

  如果使用readme.md命名，那么始终为 `path.join(__dirname, 'readme.md')`



#### 学习二：输入输出配置

```js
/**
* 输入的锚点
* 包含所有输入，与python输入的args对应
*/
inAnchor: [],
/**
* 输出的锚点
* 与python最后return的列表对应
* 但是return的长度可以超过锚点数量，return前面的变量必须与锚点对应
* 超出锚点个数的return不能作为其余模块的输入，但是可以作为可视化用
*/
outAnchor: [
	{ type: 'ndarray', label: '数组' }
],
```

按照顺序指定

- type表示对应的数据类型，==可以是字符串或者字符串数组，数组表示支持多种类型，任意类型用Any==
- label表示锚点展示的标签



#### 学习三：配置参数设置

用户可以通过配置参数给定模块的某些输入和配置

```js
/**
* 模块配置参数的值
* 对应python输入的kwargs
*/
kwargs: {
	key1: default1,
	key2: default2,
	....: ....
}
```

指定有哪些参数是容易的，除此之外需要指定这些参数的前端输入方式

输入方式不仅关联了数据key1和key2的类别，还指出了对应类别的输入形式，可以是下拉框、颜色选择器、数字输入、文本输入等。因此对应有输入的配置字段： 

```js
kwargsSchemaProperties:{
    key1: {
        ...
    },
    key2:{
        ...
	}
}
```

==目前支持的类型如下==

>读取文件路径

```js
key: {
    type: 'readFile',
    title: '文件'
}
```

> 布尔类型

```js
key: {
    type: 'boolean',
    title: '标题'
}
```
> 枚举类型

```js
key:{
    type: 'enum',
    enum:[value1,vaule2,...],
    title: '标题'，
    bind：{
    	扩展属性 select 控件    
    }
}
```

> 数组

目前用于特征选择，用穿梭框

```js
key:{
    type: 'array',
    items:[可选列表],
    title: '标题'
}
```

> Object

用于创建包裹框，将参数划分为不同组，参见LSTM案例

```
key:{
    type: 'object',
    title: '标题',
    properties:{
    	子选项
    }
}
```

> 数字

```js
key:{
    type: 'integer' or 'number',
    bind:{
      数字输入控件可选参数  
    },
    title: '标题'
}
```



#### 学习四： 可视化配置

可视化是模块计算完成后通过图表等类型展示其计算结果或者计算过程的手段，可以配置多种可视化方式

案例如下：

```js
outputVisualization: [
    {
        icon: 'icon-data',
        label: '预测值',
        // 使用的output的下标，是一个列表，表示可以集成多个输出
        usedKeys: [0, 1],
        // 使用的转换器, 串联, 是一个python的类
        transformer: [
            {
                modulePath: 'Transformer.List2BarChart',
                moduleKey: 'transform'
            }
        ],
        // 需要复写的key, 可以是多个
        propKey: [
            ['options']
        ],
        type: 'chart',
        // 可以定制的props
        props: {
            options: {}
        }
    }
]
```

可见可视化是一个配置列表，不同配置没有关联

- 配置图标 icon ==图片路径==
- 配置标题 label

可视化是根据模块的输出(return的数组)转化为可视化的配置，所以需要配置使用的输出参数下标，比如每一个输出都可以配置一个可视化，或者使用多个输出配置一个可视化

- 使用的输出下标 usedKeys 是一个列表

- 转换器 transformer

  是一个列表, 后面一个transformer以上一个transformer的输出作为输入，第一个transformer使用usedKeys对应的模块输出作为输入，最后一个transformer输出层为可视化的配置

- 转化器输出对应的可视化配置 propKey

  propKey的长度和最后一个transformer输出长度一致，分别复写了一个prop

- 可视化的类型选择 type

  目前提供了chart，也就是图表选项，还有表格

- 对应可视化类型配置对象 props

  chart对应的配置项就是options
  
  table需要配置columns和dataSource



**逻辑梳理**：

假设模块输出是 [A,B,C,D]

usekeys: [0,2]

那么transformer的输入就是[A,C]

经过transformer后输出为O1

对应的propKey 为['key1','key2','key3']

那么O1将复写props的对应字段，也就是`props.key1.key2.key3=O1` 

然后前端根据props进行展示



**疑问：为什么transformer是一个列表？**

不同transformer之间是串联关系，可以拼合多个转换器 TypeA->TypeB->TypeC



**疑问：transformer如何写？**

transformer配置对应的地址为modulePath和muduleKey, 应该统一放在Transformer文件夹下

命名规则为typeA2typeB.py

参考如下：

```python
import pandas as pd


def transform(*args, **kwargs):
    series = []
    df = args[0]
    for key in df.keys():
        series.append({
            "name": key,
            "type": 'line',
            "data": df[key].tolist()
        })
    options = {
        "tooltip": {
            "trigger": 'axis'
        },
        "xAxis": {
            "type": 'category',
            "data": df.index.tolist()
        },
        "yAxis": {
            "type": 'value'
        },
        "series": series
    }
```

如果是绘图的options, 参照echarts文档



#### 学习五：训练和应用的个性化

训练和应用配置可能是不一样的，比如可视化配置，比如输入和输出配置

案例：

LSTM训练的时候最后可以展示loss变化，而应用的时候没有loss变化曲线，可视化不一样；配置输入的时候也无需再配置网络结构参数，只需要加载模型

个性化配置如下：

```js
module.exports = {
    default:{
        ...默认的配置
    },
    development:{
        ...开发环境应该复写的配置
    },
    production:{
        ...应用环境应该复写的配置
    }
}
```



#### 学习六：弹窗设置

```json
kwargsModelSettings: {
    contentSize: {
        height: '480px',
        width: '260px'
    },
    // object or false
    /**
       *  aspectRatio type: string, default: undefined
       *  disable type: boolean, default: undefined
       *  handles type: string, default: 'n, e, s, w, ne, se, sw, nw' 控制点
       *  minWidth Number 128
       *  minHeight Number 38
       *  maxWidth Number undefined
       *  maxHeight Number undefined
       */
    resizeit: false //以上是可选参数，一般固定大小
}
```



#### 学习七： 参数初始化

模块配置参数可能取决于输入，因此打开设置的时候需要初始化，参见特征选择案例

```js
KwargsInitialize: [
    {
        transformer: [
            {
                modulePath: 'Transformer.GetDFColumns',
                moduleKey: 'transform'
            }
        ],
        useInputIndex: [0],
        // 需要复写的key, 可以是多个
        propKey: [
            ['selectColumns', 'items']
        ]
    }
]
```

和可视化配置一样，不一样的地方是可视化修改的是outputVisualization子 对象，而此处初始化修改的是kwargs

















