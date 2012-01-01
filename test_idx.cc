#include <iostream>
#include <stdexcept>
#include <sstream>
#include "idx.hh"
#include "ensure.hh"

void test_simple_index()
{
	index_st idx_st("tmp.idx");
	stream s("data/short.xml", region(0, 0));
	
	index_result r1(index_article(s, idx_st));
	ENSURE(r1 == INDEX_GOOD);
	ENSURE(idx_st.article_count() == 1);
	ENSURE(idx_st.has_article("April"));
	ENSURE(idx_st.is_associated("April", "april"));
	ENSURE(idx_st.is_associated("April", "fourth"));
	ENSURE(idx_st.is_associated("April", "month"));
	ENSURE(idx_st.is_associated("April", "chuispastonbot"));
	ENSURE(idx_st.is_associated("April", "easter"));
	ENSURE(idx_st.is_associated("April", "australian"));
	ENSURE(!idx_st.is_associated("April", "the"));
	
	index_result r2(index_article(s, idx_st));
	ENSURE(r2 == INDEX_GOOD);
	ENSURE(idx_st.article_count() == 2);
	ENSURE(idx_st.has_article("August"));
	ENSURE(idx_st.is_associated("August", "august"));
	ENSURE(idx_st.is_associated("August", "eighth"));
	ENSURE(idx_st.is_associated("August", "month"));
	ENSURE(idx_st.is_associated("August", "sextilis"));
	ENSURE(!idx_st.is_associated("August", "citation"));
	
	ENSURE(index_article(s, idx_st) == INDEX_GOOD);
	ENSURE(idx_st.has_article("Art"));
	ENSURE(idx_st.article_count() == 3);
	ENSURE(index_article(s, idx_st) == INDEX_GOOD);
	ENSURE(idx_st.has_article("A"));
	ENSURE(idx_st.article_count() == 4);
	ENSURE(index_article(s, idx_st) == INDEX_GOOD);
	ENSURE(idx_st.has_article("Air"));
	ENSURE(idx_st.article_count() == 5);
	ENSURE(index_article(s, idx_st) == END_OF_REGION);
	ENSURE(idx_st.article_count() == 5);
	ENSURE(index_article(s, idx_st) == END_OF_REGION);
	ENSURE(idx_st.article_count() == 5);
	
	ENSURE(idx_st.is_associated("Art", "poetry"));
	// more...
	
	system("rm tmp.idx*");
}

int main()
{
	int rc(0);
	try {
		test_simple_index();
		std::cout << "success" << std::endl;
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}

