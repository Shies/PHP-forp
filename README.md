# Introduction #

PHP7-forp分析器是一个轻量级的PHP扩展提供脚本的调用堆栈,CPU和内存使用.

总结的特性 :
- PHP7编译时要使用(--enable-dtrace)
- PHP7使用时需要设置环境变量(export USE_ZEND_DTRACE=1)
- 每个函数的时间和分配内存的测量
- CPU使用率
- 函数调用的文件和行号
- 输出为谷歌的跟踪事件格式
- 字幕功能
- 分组函数
- 函数别名（对匿名函数有用）


forp是非侵入性的,它提供了PHP注释来完成工作.

# 简单例子 #

Example :
```php
<?php
// 启用分析器
forp_start();

// 这里, 我想要去分析这个函数
function foo()
{
    echo "Hello world !\n";
};

foo();

// 停止分析器
forp_end();

// 获取一个堆栈数组
$profileStack = forp_dump();

print_r($profileStack);
```

Result :
```
Hello world !
Array
(
    [utime] => 125
    [stime] => 18
    [stack] => Array
        (
            [0] => Array
                (
                    [file] => /Users/bilibili/debug/PHP7-forp/ext/forp/examples/hello.php
                    [function] => {main}
                    [usec] => 99
                    [pusec] => 13
                    [bytes] => 48
                    [level] => 0
                )

            [1] => Array
                (
                    [file] => /Users/bilibili/debug/PHP7-forp/ext/forp/examples/hello.php
                    [function] => foo
                    [lineno] => 11
                    [usec] => 42
                    [pusec] => 0
                    [bytes] => 48
                    [level] => 1
                    [parent] => 0
                )

        )

)
```

# 例如与注释 #

Example :
```php
<?php
// 启用分析器
forp_start();

/**
 * 这里，我们要介绍的PHP代码
 * 注释
 *
 * @ProfileGroup("Test")
 * @ProfileCaption("Foo #1")
 * @ProfileAlias("foo")
 */
function fooWithAnnotations($bar)
{
    return 'Foo ' . $bar;
}

echo "foo = " . fooWithAnnotations("bar") . "\n";

// 停止分析器
forp_end();

// 获取一个堆栈数组
$profileStack = forp_dump();

echo "forp stack = \n";
print_r($profileStack);
```

Result :
```
foo = Foo bar
forp stack =
Array
(
    [utime] => 269
    [stime] => 24
    [stack] => Array
        (
            [0] => Array
                (
                    [file] => /Users/bilibili/debug/PHP7-forp/ext/forp/forp.php
                    [function] => {main}
                    [usec] => 247
                    [pusec] => 15
                    [bytes] => 48
                    [level] => 0
                )

            [1] => Array
                (
                    [file] => /Users/bilibili/debug/PHP7-forp/ext/forp/forp.php
                    [function] => foo
                    [lineno] => 18
                    [groups] => Array
                        (
                            [0] => Test
                        )

                    [caption] => Foo bar
                    [usec] => 28
                    [pusec] => 145
                    [bytes] => 80
                    [level] => 1
                    [parent] => 0
                )

        )

)
```

# php.ini 选项 #

- forp.max_nesting_level : 默认 50
- forp.no_internals : 默认 0

# forp PHP API #

- forp_start(flags*) : 开始forp收集器
- forp_end() : 停止forp收集器
- forp_dump() : 返回堆栈为平面数组
- forp_print() : 显示forp栈 (SAPI CLI)

## forp_start() 标记 ##

- FORP_FLAG_CPU : 激活时间收集
- FORP_FLAG_MEMORY : 激活内存使用的收集
- FORP_FLAG_CPU : 获取cpu使用率
- FORP_FLAG_CAPTION : 启用文字处理程序
- FORP_FLAG_ALIAS : 启用别名处理程序
- FORP_FLAG_GROUPS : 启用组处理程序
- FORP_FLAG_HIGHLIGHT : 启用HTML高亮显示

## forp_dump() ##

forp_dump() 提供了一个数组组成 :

- global fields : utime, stime ...
- stack : 一个扁平的堆栈条目PHP数组.

Global fields :

- utime : 用户函数调用CPU使用
- stime : 系统调用CPU使用

一个堆栈条目 :

- file : 文件名
- lineno : 所在行号
- class : 类名
- function : 方法名
- groups : 关联组列表
- caption : 函数的说明
- usec : 函数的时间(没有剖析开销)
- pusec : 内部分析时间(没有执行功能)
- bytes : 内存使用情况的功能
- level : 从forp_start调用深度水平
- parent : 父指数(数字)

## forp_json() ##

在标准输出上直接打印堆栈为JSON字符串。
这是最快的方法来发送JSON兼容的客户端堆栈.

看forp_dump()它的结构.

## forp_json_google_tracer() ##

forp_json_google_tracer($filepath) 输出为谷歌的跟踪事件格式.

Usage :
```php
// 启用分析器
forp_start();

my_complex_function();

// 停止分析器
forp_end();

// 获取Json和保存它到文件里
forp_json_google_tracer("/tmp/output.json");
```
然后, 打开谷歌浏览器输入 chrome://tracing/. 载入输出文件并观看结果.

![Example output with an existing Drupal website](/docs/images/google-tracing-example_thumb.png?raw=true "Google Tracing Format example output")


## forp_inspect() ##

forp_inspect('symbol', $symbol) 将输出的详细表示在forp_dump()结果变量.

Usage :
```php
$var = array(0 => "my", "strkey" => "inspected", 3 => "array");
forp_inspect('var', $var);
print_r(forp_dump());
```

Result :
```php
Array
(
    [utime] => 0
    [stime] => 0
    [inspect] => Array
        (
            [var] => Array
                (
                    [type] => array
                    [size] => 3
                    [value] => Array
                        (
                            [0] => Array
                                (
                                    [type] => string
                                    [value] => my
                                )

                            [strkey] => Array
                                (
                                    [type] => string
                                    [value] => inspected
                                )

                            [3] => Array
                                (
                                    [type] => string
                                    [value] => array
                                )

                        )

                )

        )

)
```

## 可用的注释 ##

- @ProfileGroup

设置属于函数的组.

```php
/**
 * @ProfileGroup("data loading","rendering")
 */
function exec($query) {
    /* ... */
}
```

- @ProfileCaption

向函数添加说明。标题字符串可能包含引用(# < param num >)的功能参数.

```php
/**
 * @ProfileCaption("Find row for pk #1")
 */
public function findByPk($pk) {
    /* ... */
}
```

- @ProfileAlias

给函数赋予别名。对匿名函数有用

```php
/**
 * @ProfileAlias("MyAnonymousFunction")
 */
$fn = function() {
    /* ... */
}
```

- @ProfileHighlight

在函数生成的输出周围添加一个框架.

```php
/**
 * @ProfileHighlight("1")
 */
function render($datas) {
    /* ... */
}
```

# 安装要求 #

## 要求 ##

php5-dev

```sh
apt-get install php5-dev
```

## 安装composer ##

要求必须在项目composer.json PHP分析器

```json
"require-dev":       {
  "gukai/php7-forp" : "dev-master"
},
"repositories" : [
  {
     "type" : "git",
     "url"  : "git@github.com:Shies/PHP7-forp.git"
  }
]
```
run Composer install
```sh
php composer.phar install
```
compile
```sh
cd vendor/Shies/PHP7-forp/ext/forp
export USE_ZEND_DTRACE=1
phpize
./configure
make && make install
```
在你的php.ini里启用forp
```sh
extension=forp.so
```
## 或选择"老式的"安装 ##

```
OR dev-master
```sh
git clone https://github.com/Shies/PHP7-forp
cd PHP7-forp/ext/forp
```
compile
```sh
export USE_ZEND_DTRACE=1
phpize
./configure
make && make install
```
并且在你的php.ini里启用forp
```sh
extension=forp.so
```

## 测试平台 ##

### MacOS ###

MacOS Sierra/10.12.3 (unix)

Nginx/1.12.0         (web)

PHP 5.6.30           (cli) (built: May 10 2017 19:52:30)

### MacOS ###

MacOS Sierra/10.12.3 (unix)

Nginx/1.12.0         (web)

PHP 7.0.7            (cli) (built: May 17 2017 19:52:30)


# 贡献者 #

[Anthony Terrien](https://github.com/aterrien/),
[Ioan Chiriac](https://github.com/ichiriac/),
[Alexis Okuwa](https://github.com/wojons/),
[TOMHTML](https://github.com/TOMHTML/),
[_____Shies](https://github.com/Shies/)
