def snake_to_title(str):
    s = str.split('_')
    return ''.join([x.title() for x in s])


def snake_to_camel(str):
    s = str.split('_')
    return ''.join([x.title() if i > 0 else x for i, x in enumerate(s)])


def make_full_name(prefix, outer=None):
    if not outer:
        return prefix
    return outer.full_name + '::' + prefix


def namespace_wrapper(n):
    def fn_wrapper(fn):
        def fn_wrapper_inner(*args, **kwargs):
            res = 'namespace %s\n{\n' % n
            res += fn(*args, **kwargs)
            res += '\n}\n'
            return res
        return fn_wrapper_inner
    return fn_wrapper
