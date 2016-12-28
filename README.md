# 教師なし形態素解析でWordCloud

NPYLMによる単語分割でワードクラウドを作ります。

以下の論文をもとに実装しています。

- [ベイズ階層言語モデルによる教師なし形態素解析](http://chasen.org/~daiti-m/paper/nl190segment.pdf)

特許の関係でソースを公開することはできないため共有ライブラリのみになります。

また特許の関係でモデルの保存機能もありません。ワードクラウドを生成する際は毎回最初からやり直しとなります。

NPYLMの実装に興味がある方は[実装方法](http://musyoku.github.io/2016/12/14/%E3%83%99%E3%82%A4%E3%82%BA%E9%9A%8E%E5%B1%A4%E8%A8%80%E8%AA%9E%E3%83%A2%E3%83%87%E3%83%AB%E3%81%AB%E3%82%88%E3%82%8B%E6%95%99%E5%B8%AB%E3%81%AA%E3%81%97%E5%BD%A2%E6%85%8B%E7%B4%A0%E8%A7%A3%E6%9E%90/)をお読みください。

# インストール

## 依存関係

Boost 1.62の環境でビルドしているため、`libboost_python.so.1.62.0`に依存します。

ない場合はインストールしておきます。

b2の場合
```
sudo ./b2 install -j4 --with-python
sudo ldconfig
```

apt-getの場合

```
sudo apt-get install libboost-python-dev
```

またPython 2.7を前提にビルドしています。

## macOSの場合

`model.mac.so`を`model.so`にリネームします。

macOSではEl Capitan以降から導入されたSIPの影響でそのままでは実行できません。

`makefile`の`install`の項目のboostのパスを自身の環境に合わせて変更し、ターミナルで

```
make install
```

すると`model.so`が修正され実行可能になります。

## Ubuntuの場合

`model.linux.so`を`model.so`にリネームします。

# 生成

 ワードクラウドを作るツールは単語分割にMeCab等の辞書ベースの形態素解析器を用いるのが一般的です。

 それに対して本ツールは与えられた文字列集合から単語を推定しワードクラウドを生成します。

 学習処理が必要なため若干時間はかかりますが、言語に関係なく文字列であれば何でも学習可能です。

## オプション

実行時の引数については`generate.py`に記載してあります。

## 実行例

`python generate.py -t voynich.txt -l 15 -e 1 -n 200 -o voinich -f 150 -c 5`

# しょこたんブログ

![image](http://musyoku.github.io/images/post/2016-12-28/syokotan.png)

# 市況

![image](http://musyoku.github.io/images/post/2016-12-28/sikyou.png)

# 源氏物語

![image](http://musyoku.github.io/images/post/2016-12-28/genji.png)

# ヴォイニッチ手稿

![image](http://musyoku.github.io/images/post/2016-12-28/voynich.png)

