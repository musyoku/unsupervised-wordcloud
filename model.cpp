#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/format.hpp>
#include "npylm/core/npylm.h"
#include "npylm/core/vocab.h"
#include "npylm/lattice/trigram.h"
#include "npylm/util.h"

using namespace boost;

template<class T>
python::list list_from_vector(vector<T> &vec){  
	 python::list list;
	 typename vector<T>::const_iterator it;

	 for(it = vec.begin(); it != vec.end(); ++it){
		  list.append(*it);
	 }
	 return list;
}

template<class T1,class T2>
python::dict dict_from_map(unordered_map<T1,T2> &map_){  
     python::dict py_dict;
     typename unordered_map<T1,T2>::const_iterator it;
     for(it = map_.begin(); it != map_.end(); ++it){
          py_dict[it->first]=it->second;        
     }
     return py_dict;  
}

// Pythonラッパー
class PyNPYLM{
private:
	HPYLM* _hpylm;
	VPYLM* _vpylm;
	NPYLM3* _npylm;
	TrigramLattice* _lattice;
	Vocab* _vocab;
	vector<wstring> _dataset;
	bool _is_npylm_ready;
	bool _is_training_ready;
	unordered_map<int, vector<int>> _old_segments_for_data;	// 古い単語分割
	unordered_map<id, int> _words_from_segmentation;			// 単語分割で得られた語彙集合
	vector<int> _rand_indices;

public:
	PyNPYLM(){
		// 日本語周り
		setlocale(LC_CTYPE, "ja_JP.UTF-8");
		ios_base::sync_with_stdio(false);
		locale default_loc("ja_JP.UTF-8");
		locale::global(default_loc);
		locale ctype_default(locale::classic(), default_loc, locale::ctype); //※
		wcout.imbue(ctype_default);
		wcin.imbue(ctype_default);

		c_printf("[*]%s\n", "NPYLMを初期化しています ...");
		_hpylm = new HPYLM(3);
		_vpylm = new VPYLM();
		_vocab = new Vocab();
		_npylm = new NPYLM3(_hpylm, _vpylm, _vocab);
		_lattice = new TrigramLattice(_npylm, _vocab);
		_is_npylm_ready = false;
		_is_training_ready = false;
	}
	// lambdaの事前分布
	void init_lambda(double alpha, double beta){
		_npylm->init_lambda(alpha, beta);
	}
	// 可能な単語の最大長
	void set_max_word_length(int length){
		_lattice->_max_length = length;
	}
	// ポアソン補正時のp(k|VPYLM)の推定を棄却する期間
	// 1イテレーション目はそもそも単語確率を求めないのでポアソン補正を行わない
	// 2イテレーション目はp(k|VPYLM)の精度が良くない
	// だいたい2〜4くらいを指定すればよい
	void set_burn_in_period_for_pk_vpylm(int period){
		_npylm->_burn_in_period_for_pk_vpylm = period;
	}
	int get_num_lines(){
		return _dataset.size();
	}
	void load_textfile(string filename){
		load_characters_in_textfile(filename, _dataset);
		// 全文字種を特定
		int max_sentence_length = 0;
		int sum_sentence_length = 0;
		for(int i = 0;i < _dataset.size();i++){
			wstring &sentence = _dataset[i];
			sum_sentence_length += sentence.size();
			for(int l = 0;l < sentence.size();l++){
				_vocab->add_character(sentence[l]);
			}
			if(sentence.size() > max_sentence_length){
				max_sentence_length = sentence.size();
			}
		}
		double average_sentence_length = sum_sentence_length / (double)_dataset.size();
		c_printf("[*]%s\n", (boost::format("%sを読み込みました. (%d行, 平均文長%lf, 最大文長%d)") % filename % _dataset.size() % average_sentence_length % max_sentence_length).str().c_str());
		double g0 = 1.0 / _vocab->get_num_characters();
		set_g0(g0);
		_lattice->init(max_sentence_length);
	}
	void set_g0(double g0){
		_vpylm->_g0 = g0;
		c_printf("[*]%s\n", (boost::format("g0 <- %lf") % g0).str().c_str());
		_is_npylm_ready = true;
	}
	vector<wstring> split_sentence_into_words(wstring &sentence, vector<int> &segments){
		int num_pieces_in_segments = accumulate(segments.begin(), segments.end(), 0);
		if(num_pieces_in_segments != sentence.size()){
			c_printf("[r]%s [*]%s %d != %d\n", "エラー:", "不正な分割です.", num_pieces_in_segments, sentence.size());
			exit(1);
		}
		vector<wstring> words;
		int pointer = 0;
		for(int i = 0;i < segments.size();i++){
			int segment_length = segments[i];
			wstring word(sentence.begin() + pointer, sentence.begin() + pointer + segment_length);
			words.push_back(word);
			pointer += segment_length;
		}
		return words;
	}
	vector<id> convert_words_to_token_ids(vector<wstring> &words){
		vector<id> token_ids;
		token_ids.push_back(_vocab->get_bos_id());
		token_ids.push_back(_vocab->get_bos_id());		// 3-gramなので2回
		for(int i = 0;i < words.size();i++){
			token_ids.push_back(_vocab->add_string(words[i]));
		}
		token_ids.push_back(_vocab->get_eos_id());
		return token_ids;
	}
	void show_segmentation_result_for_sentence(wstring &sentence, vector<int> &segments){
		_lattice->perform_blocked_gibbs_sampling(sentence, segments, true);
		vector<wstring> words = split_sentence_into_words(sentence, segments);
		for(int i = 0;i < words.size();i++){
			wcout << words[i] << L" / ";
		}
		wcout << endl;
	}
	void show_segmentation_result_for_data(int data_index){
		vector<int> segments;
		wstring &sentence = _dataset[data_index];
		show_segmentation_result_for_sentence(sentence, segments);
	}
	void show_random_segmentation_result(int num_generation = 30){
		shuffle(_rand_indices.begin(), _rand_indices.end(), Sampler::mt);
		vector<int> segments;
		for(int step = 0;step < min(_dataset.size(), num_generation);step++){
			int data_index = _rand_indices[step];
			wstring &sentence = _dataset[data_index];
			show_segmentation_result_for_sentence(sentence, segments);
		}
	}
	void prepare_for_training(){
		if(_is_npylm_ready == false){
			c_printf("[r]%s [*]%s\n", "エラー:", "学習に必要なものが揃っていません.");
			exit(1);
		}
		for(int data_index = 0;data_index < _dataset.size();data_index++){
			_rand_indices.push_back(data_index);
		}
		_npylm->init_pk_vpylm(_lattice->_max_length);
		_is_training_ready = true;
	}
	void perform_gibbs_sampling(){
		if(_is_training_ready == false){
			c_printf("[r]%s [*]%s\n", "エラー:", "学習の準備ができていません.");
			exit(1);
		}
		int num_lines = get_num_lines();
		vector<int> segments;									// 分割の一時保存用
		shuffle(_rand_indices.begin(), _rand_indices.end(), Sampler::mt);		// データをシャッフル
		_words_from_segmentation.clear();					// 語彙集合をリセット
		// モデルパラメータを更新
		for(int step = 0;step < num_lines;step++){
			show_progress(step, num_lines);
			// 訓練データを一つ取り出す
			int data_index = _rand_indices[step];
			wstring &sentence = _dataset[data_index];
			// 古い分割を取得
			auto itr_segments = _old_segments_for_data.find(data_index);
			if(itr_segments == _old_segments_for_data.end()){
				// 最初は<bos>,<eos>を除く全ての文字列が1単語としてモデルに追加される
				segments.clear();
				segments.push_back(sentence.size());
			}else{
				// 古い分割をモデルから削除
				vector<wstring> old_words = split_sentence_into_words(sentence, itr_segments->second);
				vector<id> old_token_ids = convert_words_to_token_ids(old_words);
				for(int token_t_index = 2;token_t_index < old_token_ids.size();token_t_index++){
					_npylm->remove_customer_at_timestep(old_token_ids, token_t_index);
				}
				// 新しい分割を取得
				_lattice->perform_blocked_gibbs_sampling(sentence, segments);
			}
			// 新しい分割結果をモデルに追加
			vector<wstring> words = split_sentence_into_words(sentence, segments);
			vector<id> token_ids = convert_words_to_token_ids(words);	// token_idsにはbosとeosが含まれる
			for(int token_t_index = 2;token_t_index < token_ids.size();token_t_index++){
				_npylm->add_customer_at_timestep(token_ids, token_t_index);
				if(token_t_index != token_ids.size() - 1){
					_words_from_segmentation[token_ids[token_t_index]] += 1;		// <eos>以外を語彙集合に追加
				}
			}
			_old_segments_for_data[data_index] = segments;
		}

		if(_npylm->_hpylm->_root->_num_tables > _npylm->_vpylm->get_num_customers()){
			c_printf("[r]%s [*]%s\n", "エラー:", "VPYLMの客数が異常です. _hpylm->_root->get_num_tables() > _vpylm->get_num_customers()");
			exit(1);
		}

		// 不要な単語IDを辞書から除去
		map<id, bool> flags;
		_npylm->set_active_tokens(flags);
		_vocab->remove_unused_token_ids(flags);
	}
	double compute_perplexity(){
		double ppl = 0;
		int num_lines = _dataset.size();
		vector<int> segments;					
		for(int data_index = 0;data_index < num_lines;data_index++){
			wstring &sentence = _dataset[data_index];
			_lattice->perform_blocked_gibbs_sampling(sentence, segments);
			vector<wstring> words = split_sentence_into_words(sentence, segments);
			vector<id> token_ids = convert_words_to_token_ids(words);
			double log_p = _npylm->log2_Pw(token_ids) / token_ids.size();
			ppl += log_p;
		}
		ppl = exp(-ppl / num_lines);
		return ppl;
	}
	void dump_lambda(){
		cout << "lambda(アルファベット) <- " << _npylm->_lambda_for_type[1] << endl;
		cout << "lambda(数字) <- " << _npylm->_lambda_for_type[2] << endl;
		cout << "lambda(記号) <- " << _npylm->_lambda_for_type[3] << endl;
		cout << "lambda(ひらがな) <- " << _npylm->_lambda_for_type[4] << endl;
		cout << "lambda(カタカナ) <- " << _npylm->_lambda_for_type[5] << endl;
		cout << "lambda(漢字) <- " << _npylm->_lambda_for_type[6] << endl;
		cout << "lambda(漢字+ひらがな) <- " << _npylm->_lambda_for_type[7] << endl;
		cout << "lambda(漢字+カタカナ) <- " << _npylm->_lambda_for_type[8] << endl;
		cout << "lambda(その他) <- " << _npylm->_lambda_for_type[9] << endl;
	}
	// HPYLM,VPYLMのdとthetaをサンプリング
	void sample_pitman_yor_hyperparameters(){
		_npylm->sample_pitman_yor_hyperparameters();
	}
	// lambdaをサンプリング
	void sample_lambda(){
		_npylm->sample_lambda(_words_from_segmentation);
	}
	// VPYLMから単語を生成し単語長の分布を更新
	void update_pk_vpylm(){
		_npylm->update_pk_vpylm(_lattice->_max_length + 1);
	}
	int get_num_nodes_of_vpylm(){
		return _npylm->_vpylm->get_num_nodes();
	}
	int get_num_customers_of_vpylm(){
		return _npylm->_vpylm->get_num_customers();
	}
	int get_depth_of_vpylm(){
		return _npylm->_vpylm->get_max_depth();
	}
	int get_num_nodes_of_hpylm(){
		return _npylm->_hpylm->get_num_nodes();
	}
	int get_num_customers_of_hpylm(){
		return _npylm->_hpylm->get_num_customers();
	}
	python::dict get_frequent_words(int threshold){
		unordered_map<id, int> count_for_id;
		for(auto elem: _npylm->_hpylm->_root->_arrangement){
			id token_id = elem.first;
			vector<int> &table = elem.second;
			double count = std::accumulate(table.begin(), table.end(), 0);	// テーブルの客数が出現頻度
			if(count < threshold){
				continue;
			}
			count_for_id[token_id] = count;
		}
		return dict_from_map(count_for_id);
	}
};

BOOST_PYTHON_MODULE(model){
	python::class_<PyNPYLM>("npylm")
	.def("load_textfile", &PyNPYLM::load_textfile)
	.def("show_random_segmentation_result", &PyNPYLM::show_random_segmentation_result)
	.def("show_segmentation_result_for_data", &PyNPYLM::show_segmentation_result_for_data)
	.def("perform_gibbs_sampling", &PyNPYLM::perform_gibbs_sampling)
	.def("compute_perplexity", &PyNPYLM::compute_perplexity)
	.def("prepare_for_training", &PyNPYLM::prepare_for_training)
	.def("sample_pitman_yor_hyperparameters", &PyNPYLM::sample_pitman_yor_hyperparameters)
	.def("sample_lambda", &PyNPYLM::sample_lambda)
	.def("dump_lambda", &PyNPYLM::dump_lambda)
	.def("init_lambda", &PyNPYLM::init_lambda)
	.def("get_num_lines", &PyNPYLM::get_num_lines)
	.def("get_depth_of_vpylm", &PyNPYLM::get_depth_of_vpylm)
	.def("get_num_customers_of_hpylm", &PyNPYLM::get_num_customers_of_hpylm)
	.def("get_num_customers_of_vpylm", &PyNPYLM::get_num_customers_of_vpylm)
	.def("get_num_nodes_of_hpylm", &PyNPYLM::get_num_nodes_of_hpylm)
	.def("get_num_nodes_of_vpylm", &PyNPYLM::get_num_nodes_of_vpylm)
	.def("set_max_word_length", &PyNPYLM::set_max_word_length)
	.def("set_burn_in_period_for_pk_vpylm", &PyNPYLM::set_burn_in_period_for_pk_vpylm)
	.def("update_pk_vpylm", &PyNPYLM::update_pk_vpylm)
	.def("get_frequent_words", &PyNPYLM::get_frequent_words);
}