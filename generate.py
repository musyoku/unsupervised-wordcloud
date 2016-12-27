# -*- coding: utf-8 -*-
import argparse, sys, time
import model

parser = argparse.ArgumentParser()
parser.add_argument("-t", "--text", type=str, help="訓練用のテキストファイル.")
parser.add_argument("-l", "--max_word_length", type=int, default=15, help="可能な単語の最大長.")
parser.add_argument("-n", "--max_word_in_cloud", type=int, default=100, help="クラウドに現れる単語の最大数.")
parser.add_argument("-w", "--width", type=int, default=800, help="クラウドの幅.")
parser.add_argument("-h", "--height", type=int, default=450, help="クラウドの高さ.")
parser.add_argument("-c", "--color", type=int, default=1, help="クラウドの配色パターン. 1か2.")
parser.add_argument("-e", "--generate_per_epoch", type=int, default=10, help="クラウドを何イテレーションごとに生成するか.")
parser.add_argument("-d", "--count_threshold", type=int, default=10, help="単語の出現頻度がこの値を下回っていれば切り捨てる.")
args = parser.parse_args()

def main():
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
			npylm.get_frequent_words(args.count_threshold)

if __name__ == "__main__":
	main()