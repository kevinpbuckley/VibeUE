# VibeUE MCP Publishing Checklist

## ✅ Completed Setup

All automated publishing infrastructure is now in place:

1. **Package Configuration**
   - ✅ `pyproject.toml` updated with proper package name (`vibeue`), metadata, and PyPI classifiers
   - ✅ MCP name marker added to README: `io.github.kevinpbuckley/vibeue`
   - ✅ `server.json` created and validated against MCP schema
   - ✅ GitHub Actions workflow configured for automated publishing

2. **Validation**
   - ✅ Schema validation passed successfully
   - ✅ Validation script created: `validate_server.py`

## 🚀 Before First Publish

### Required Actions (One-Time Setup)

1. **Create PyPI Account** (if not already done)
   - Go to https://pypi.org/account/register/
   - Verify email address

2. **Create PyPI API Token**
   - Go to https://pypi.org/manage/account/token/
   - Create a new token with scope: "Entire account (all projects)"
   - Copy the token (starts with `pypi-`)

3. **Add GitHub Secret**
   - Go to https://github.com/kevinpbuckley/VibeUE/settings/secrets/actions
   - Click "New repository secret"
   - Name: `PYPI_API_TOKEN`
   - Value: [paste the PyPI token]
   - Click "Add secret"

4. **Verify GitHub Actions Permissions**
   - Go to https://github.com/kevinpbuckley/VibeUE/settings/actions
   - Under "Workflow permissions", ensure "Read and write permissions" is enabled
   - Ensure "Allow GitHub Actions to create and approve pull requests" is enabled

### Publishing Your First Release

Once the one-time setup is complete:

```bash
# 1. Ensure you're on the main/master branch with latest changes
git checkout master
git pull

# 2. Create a version tag (e.g., v0.1.0)
git tag v0.1.0

# 3. Push the tag to trigger automated publishing
git push origin v0.1.0
```

### What Happens Automatically

The GitHub Actions workflow will:
1. ✅ Build the Python package
2. ✅ Publish to PyPI at https://pypi.org/project/vibeue/
3. ✅ Authenticate with MCP Registry using GitHub OIDC (no token needed!)
4. ✅ Publish to MCP Registry at https://registry.modelcontextprotocol.io/
5. ✅ Create a GitHub Release with installation instructions

### After Publishing

**Verify publication:**

1. **PyPI**: https://pypi.org/project/vibeue/0.1.0/
2. **MCP Registry**: 
   ```bash
   curl "https://registry.modelcontextprotocol.io/v0/servers?search=io.github.kevinpbuckley/vibeue"
   ```

**Test installation:**
```bash
pip install vibeue
```

## 📝 Version Updates

For subsequent releases:

1. Update version in `pyproject.toml` (e.g., `0.1.0` → `0.2.0`)
2. Commit the version change
3. Create and push a new tag:
   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```

The workflow automatically updates `server.json` version to match the tag.

## 🔧 Manual Publishing (Fallback)

If you need to publish manually:

```bash
# Navigate to Python directory
cd Plugins/VibeUE/Python/vibe-ue-main/Python/

# Build package
python -m build

# Publish to PyPI
python -m twine upload dist/*

# Install MCP Publisher
curl -L "https://github.com/modelcontextprotocol/registry/releases/latest/download/mcp-publisher_linux_amd64.tar.gz" | tar xz mcp-publisher

# Login with GitHub
./mcp-publisher login github-oidc

# Publish to MCP Registry
./mcp-publisher publish
```

## 🐛 Troubleshooting

**"Authentication failed" on PyPI**: Check that PYPI_API_TOKEN secret is set correctly

**"Package validation failed" on MCP**: Ensure README contains `mcp-name: io.github.kevinpbuckley/vibeue`

**"Schema validation failed"**: Run `python validate_server.py` to check server.json

**"OIDC authentication failed"**: Ensure workflow has `id-token: write` permission (already configured)

## 📚 References

- [MCP Publishing Guide](https://raw.githubusercontent.com/modelcontextprotocol/registry/refs/heads/main/docs/guides/publishing/publish-server.md)
- [GitHub Actions Publishing](https://raw.githubusercontent.com/modelcontextprotocol/registry/refs/heads/main/docs/guides/publishing/github-actions.md)
- [PyPI Publishing Guide](https://packaging.python.org/en/latest/guides/publishing-package-distribution-releases-using-github-actions-ci-cd-workflows/)
