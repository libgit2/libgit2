GIT_INLINE(void) reset_checkout_opts(git_checkout_opts *opts)
{
	git_checkout_opts init_opts = GIT_CHECKOUT_OPTS_INIT;
	memmove(opts, &init_opts, sizeof(git_checkout_opts));
}
