# chaos

## Project setup
```
yarn install
```

### Compiles and hot-reloads for development
```
yarn serve
```

### Compiles and minifies for production
```
yarn build
```

### Lints and fixes files
```
yarn lint
```

### Customize configuration
See [Configuration Reference](https://cli.vuejs.org/config/).



窗口设置颜色


```
const chalk = window.require('chalk')
const forcedChalk = new chalk.Instance({
enabled: true,
	level: 2
})
```



选择文件夹, openFile类别

```json
options:{
	properties: ['openDirectory'],
}
```

