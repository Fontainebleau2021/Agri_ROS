### Python 模块开发指南

#### 学习一： 目录结构

```
-model 						模型所在文件夹
	-package 				模型包，不同分析可以封装为不同的包
		-class1				模块分类1
			-module1		模块文件夹
				-readme.md	模块说明文件
				-index.js	模块前端配置文件
				-xxx.py		模块定义文件
			-module2		
				...
			-index.js		class1配置文件
		-class2	
			...
		-index.js			package的配置文件
	-Transformer			python数据类型转换器
		-typeA2typeB.py		从类型A到类型B的转化器
    -XXX.py					执行入口文件
```

请使用`VS Code | Pycharm` 等工具打开`\resources\app\core\model` 目录进行开发，暂时直接在目录中增删查改模块，不提供前端创建的功能



#### 学习二：Package配置文件

```js
const glob = require('glob')
const path = require('path')

const components = []

glob.sync(path.join(__dirname, '/*/*.js')).forEach(function (file) {
  components.push(require(path.resolve(file)))
})

module.exports = {
  icon: 'icon-data',
  label:'标签',
  components
}
```

唯一要改的地方就是图标，每个package有自己的图标 icon

##### 注： ==图标为图片路径==



#### 学习三： class 配置文件

```js
const glob = require('glob')
const path = require('path')

const components = []

glob.sync(path.join(__dirname, '/*/*.js')).forEach(function (file) {
  components.push(require(path.resolve(file)))
})

module.exports = {
  label: '文件',
  components
}
```

只需要配置label字段，表明分类的名称



#### 学习四： module的开发

module 开发需要写几个文件

```
xxx.js			前端显示相关联
xxx.py			python函数
xxx.md 			说明文档
```

py和js我们将在`Py Reference.md` 和`JS Reference.md` 中单独展开介绍

说明文档尽量指出以下内容：

- 模块的名称
- 模块的理论来源
- 模块的输入和输出含义
- 模块的可视化内容含义
- 模块的使用注意事项
- 模块的使用案例



如果是在现有的package某一个分类下新加模块，那么只需要关注module开发