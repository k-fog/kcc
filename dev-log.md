# 進捗

## 2025-07-02
- 電卓コンパイラができた
  - 四則演算
  - 括弧による優先順位付け
  - 単項`+`/`-`
- Pratt Parsing
  - https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
  - https://github.com/bradford-hamilton/monkey-lang/blob/master/parser/parser.go
  - https://github.com/TheAlukard/Pratt-Parsing

## 2025-07-03
- 比較演算子を実装した
- 単項`!`
  - !x <=> x==0
  - x86_64の`cmp`っていろいろなオペランドをとれるのか、すごい
- 代入演算子`=`の構文解析
  - `=`は右結合なのでこのままではうまくいかない
  - YouTubeで[Pratt Parsingのわかりやすいアニメーションと解説](https://youtu.be/0c8b7YfsBKs?si=caL7g6vM6oz02IS4)を見つけた
  - 演算子ごとに左結合か右結合かを指定することでパースできた
- '1<2<3'みたいなのがコンパイルできてしまう
- codegenは明日やる

```sh
% ./kcc 'a=b=1+2+3'
a       TokenTag=15
=       TokenTag=0
b       TokenTag=15
=       TokenTag=0
1       TokenTag=14
+       TokenTag=1
2       TokenTag=14
+       TokenTag=1
3       TokenTag=14
        TokenTag=16
(= a (= b (+ (+ 1 2) 3)))
```

## 2025-07-04
- `NodeList`構造体を作った
- 複数の文`stmt := (expr ';')*`をパースできた
- [CompilerBook ステップ9：1文字のローカル変数](https://www.sigbus.info/compilerbook#ステップ91文字のローカル変数)を読んだ
- テストが通らないと思ったら変数のスタックのアドレス計算式（e.g. rbp-8, rbp-16）が間違ってた
- 1文字のローカル変数を使ったプログラムが動いた。`a=1;b=2;a+b; => 3`

## 2025-07-05
- Lexerで識別子を切り出す処理がバグってる。1文字の時はうまく動いていたけど複数文字になると動かない
  - while文の条件が間違っていた。なぜ今まで気づかなかった…
- 複数文字のローカル変数が使えるようになった
- 'return'キーワードを追加
- `1 2 3;`みたいな誤ったプログラムでエラーを出せるように`is_infix`関数を追加し、構文解析時にチェックする
  - `enum OpPrecedence`に`PREC_NONE = 0`を追加
  - `precedences`配列をトークンの種類数だけ確保するように変更、
    グローバル変数は0で初期化されるので、優先度を指定したトークン以外は`PREC_NONE`になる
  - `precedences[tag]`が`PREC_NONE`か否かで判定
- 関数呼び出しのパースを実装した
  ```sh
  % ./kcc 'func(a, b+c, d);'
  (func a (+ b c) d)
  ```
- `make debug`をできるようにした。
  debug用にビルドしたバイナリの場合は、テストを走らせる前に手動で`make clean`をする

## 2025-07-06
- `printf("%.*s", len, ptr)`で出力する文字列の長さを指定できるらしいので、この書き方に統一
- 引数6個までの関数呼び出しができた
  - TODO
    > 関数呼び出しをする前にRSPが16の倍数になっていなければいけません。
  - push/popでrspは8ずつ増減する。カウントする必要がありそう
  - gccを参考にローカル変数のために確保するスタックサイズを16の倍数にアラインメントするよう変更
- if文を実装
