# -*- coding: utf-8 -*-
import argparse, sys, time, random, os, re
from PIL import Image
from wordcloud import WordCloud, ImageColorGenerator
import model

parser = argparse.ArgumentParser()
parser.add_argument("-t", "--text", type=str, help="訓練用のテキストファイル.")
parser.add_argument("-l", "--max_word_length", type=int, default=15, help="可能な単語の最大長.")
parser.add_argument("-n", "--max_words_in_cloud", type=int, default=100, help="クラウドに現れる単語の最大数.")
parser.add_argument("-o", "--output_dir", type=str, default="cloud", help="出力先のフォルダのパス.")
parser.add_argument("--width", type=int, default=800, help="クラウドの幅.")
parser.add_argument("--height", type=int, default=450, help="クラウドの高さ.")
parser.add_argument("--color", type=int, default=1, help="クラウドの配色パターン. 1か2.")
parser.add_argument("-f", "--max_font_size", type=int, default=150, help="最大フォントサイズ.")
parser.add_argument("-e", "--generate_per_epoch", type=int, default=3, help="クラウドを何イテレーションごとに生成するか.")
parser.add_argument("-c", "--count_threshold", type=int, default=10, help="単語の出現頻度がこの値を下回っていれば切り捨てる.")
args = parser.parse_args()

def color_func_1(word, font_size, position, orientation, random_state=None, **kwargs):
    colors = (
        "rgb(192, 57, 43)",
        "rgb(231, 76, 60)",
        "rgb(243, 156, 18)",
        "rgb(241, 196, 15)",
        "rgb(142, 68, 173)",
        "rgb(155, 89, 182)",
        "rgb(202, 44, 104)",
        "rgb(234, 76, 136)",
        "rgb(44, 62, 80)",
        "rgb(52, 73, 94)",
        "rgb(41, 128, 185)",
        "rgb(52, 152, 219)",
        "rgb(52, 152, 219)",
        "rgb(22, 160, 133)",
        "rgb(26, 188, 156)",
        )
    index = random.randint(0, len(colors) - 1)
    return colors[index]

def color_func_2(word, font_size, position, orientation, random_state=None, **kwargs):
    colors = (
        "rgb(109, 109, 207)",
        "rgb(189, 158, 56)",
        "rgb(97, 120, 55)",
        "rgb(205, 219, 156)",
        "rgb(160, 103, 100)",
        "rgb(216, 96, 107)",
        "rgb(0, 0, 0)",
        "rgb(133, 60, 57)",
        "rgb(208, 107, 189)",
        "rgb(157, 157, 222)",
        "rgb(124, 64, 115)",
        "rgb(112, 132, 66)",
        "rgb(231, 186, 81)",
        "rgb(231, 203, 148)",
        )
    index = random.randint(0, len(colors) - 1)
    return colors[index]

def main():
	# 環境に合わせてフォントのパスを指定する。
	font_path = "/Users/htc/Desktop/fonts/.Hiragino Kaku Gothic Interface W5.otf"

	try:
		os.mkdir(args.output_dir)
	except:
		pass

	npylm = model.npylm()
	npylm.load_textfile(args.text)
	npylm.set_max_word_length(args.max_word_length)		# 可能な単語の最大長
	npylm.init_lambda(1, 1)				# lambdaの事前分布（ガンマ分布）のハイパーパラメータ

	# VPYLMから長さkの単語が生成される確率p(k|vpylm)の推定結果の棄却期間.
	# ギブスイテレーションがこの回数以下の場合は単語0-gram確率のポアソン補正を行わない.
	# 1イテレーション目は文章が丸ごと1つの単語としてモデルに追加されるので単語確率を求めることがない
	# 2イテレーション目はp(k|VPYLM)の精度が悪いので棄却
	# それ以降はお好み
	npylm.set_burn_in_period_for_pk_vpylm(4)

	npylm.prepare_for_training()
	max_epoch = 1000
	num_lines = npylm.get_num_lines()
	for epoch in xrange(1, max_epoch):
		start_time = time.time()

		# パラメータの更新
		npylm.perform_gibbs_sampling()

		# ハイパーパラメータの推定
		npylm.sample_pitman_yor_hyperparameters()
		npylm.sample_lambda()
		npylm.update_pk_vpylm()

		elapsed_time = time.time() - start_time
		print "Epoch {} - {} lps".format(
			epoch, 
			num_lines / elapsed_time
		)

		if epoch % args.generate_per_epoch == 0:
			# ストップワードの設定
			# WordCloundの引数ではなぜか効かないので自作する必要あり
			stop_words = ()
			ignore_words = ("it", "and", "the", "to", "that", "she", "he", "her", "in")
			_ignore_words = dict((key, True) for key in ignore_words)

			_words = npylm.get_frequent_words(args.count_threshold)
			words = []
			if len(_words) > 0:
				for word in _words:
					# 3文字以下の単語は漢字カタカナを含まないものだけ除去
					if len(word) < 4:
						if re.search(ur"[一-龥ァ-ン]", word) is  None:
							continue
					if word in _ignore_words:
						continue
					count = _words[word]
					words.append((word, count))

				words = dict(words)

				wordcloud = WordCloud(
					background_color="white",
					font_path=font_path, 
					width=args.width, 
					height=args.height, 
					stopwords=set(stop_words), 
					max_words=args.max_words_in_cloud, 
					max_font_size=args.max_font_size).generate_from_frequencies(words)
				color_func = color_func_1
				if args.color == 2:
					color_func = color_func_2
				wordcloud.recolor(color_func=color_func)
				wordcloud.to_file("{}/epoch_{}.png".format(args.output_dir, epoch))

if __name__ == "__main__":
	main()