if filename is None:
    return None

if filename.startswith(b'src/core/'):
    return b'include/kalos/' + filename[len(b'src/core/'):]
if filename.startswith(b'tools/native-tests/'):
    directory = b'tests/' if filename.endswith(b'.cpp') else b'docs/contracts/'
    return directory + filename[len(b'tools/native-tests/'):]
if filename.startswith(b'tools/reference/'):
    return b'validation/reference/' + filename[len(b'tools/reference/'):]
if filename.startswith(b'inst/tinytest/fixtures/'):
    return b'validation/fixtures/' + filename[len(b'inst/tinytest/fixtures/'):]
if filename.startswith(b'R/native'):
    return b'validation/migration/original-r/' + filename[len(b'R/'):]
if filename == b'IMPLEMENTATION_STATUS.md':
    return b'docs/history/persephone-implementation-baseline.md'
if filename == b'DESCRIPTION':
    return b'validation/migration/source-DESCRIPTION'
return filename
