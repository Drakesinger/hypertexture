#include "render.hh"
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <cctype>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "matrix.hh"
#include "common.hh"
#include "commonmath.hh"
#include "vec.hh"

////////////////////////////////////////////////////////////////////////////////
// File globals
static const char kVersion130[] = "#version 150\n";
static std::vector<std::shared_ptr<ShaderInfo>> g_shaders;
	
Geom::Geom(int numVerts, const float* verts, 
		int numIndices, const unsigned short* indices,
		int vertStride, int glPrimType, 
		const std::vector<GeomBindPair>& elements)
	: m_stride(vertStride)
	, m_glPrimType(glPrimType)
	, m_numIndices(numIndices)
	, m_elements(elements)
{
	glGenBuffers(2, m_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_buffer[VTX_BUFFER]);
	glBufferData(GL_ARRAY_BUFFER, numVerts * vertStride, verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer[IDX_BUFFER]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(unsigned short), indices, GL_STATIC_DRAW);
	checkGlError("Geom::Geom");
}

Geom::~Geom()
{
	glDeleteBuffers(2, m_buffer);
}

void Geom::Render(const ShaderInfo& shader)
{
	Bind(shader);
	Submit();
	Unbind(shader);
	checkGlError("Geom::Render");
}
	
void Geom::Bind(const ShaderInfo& shader)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_buffer[VTX_BUFFER]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer[IDX_BUFFER]);
	for(const GeomBindPair& pair : m_elements)
	{
		if(shader.m_attrs[pair.m_attr] >= 0)
		{
			glEnableVertexAttribArray(shader.m_attrs[pair.m_attr]);
			glVertexAttribPointer(shader.m_attrs[pair.m_attr], pair.m_count, 
					GL_FLOAT, GL_FALSE, m_stride, ((char*)(0) + pair.m_offset));
		}
	}
}

void Geom::Submit()
{
	glDrawElements(m_glPrimType, m_numIndices, GL_UNSIGNED_SHORT, 0);
}

void Geom::Unbind(const ShaderInfo& shader)
{
	for(const GeomBindPair& pair : m_elements)
		if(shader.m_attrs[pair.m_attr] >= 0)
			glDisableVertexAttribArray(shader.m_attrs[pair.m_attr]);
}

std::shared_ptr<Geom> render_GenerateBoxGeom()
{
	static float verts[] = {
		2.f, 2.f, -2.f,
		-2.f, 2.f, -2.f,
		-2.f, -2.f, -2.f,
		2.f, -2.f, -2.f,
		2.f, 2.f, 2.f,
		-2.f, 2.f, 2.f,
		-2.f, -2.f, 2.f,
		2.f, -2.f, 2.f,
	};

	static unsigned short indices[] = {
		// quad 0,1,2,3
		0, 1, 2,
		0, 2, 3,
		// quad 4,7,6,5
		4, 7, 6,
		4, 6, 5,
		// quad 0,3,7,4
		0, 3, 7,
		0, 7, 4,
		// quad 1,5,6,2
		1, 5, 6,
		1, 6, 2,
		// quad 2,6,7,3
		2, 6, 7,
		2, 7, 3,
		// quad 0,4,5,1
		0, 4, 5,
		0, 5, 1,
	};

	return std::make_shared<Geom>(
		ARRAY_SIZE(verts)/3, &verts[0],
		ARRAY_SIZE(indices), &indices[0],
		3*sizeof(float), GL_TRIANGLES,
		std::vector<GeomBindPair>{ {GEOM_Pos, 3, 0} }
	);
}

std::shared_ptr<Geom> render_GenerateSphereGeom(int subdivH, int subdivV)
{
	const int kN = Max(subdivV, 2), kM = Max(subdivH, 4);
	const int numVerts = 2 + (kN - 2) * kM;
	const int numFacesInStrip = 2*kN - 4;
	const int numFaces = kM * numFacesInStrip;
	int n, m; // m is around theta (+x to +y to -x to -y), n is around phi (+z to -z)
	int off;
	std::vector<float> verts(6*numVerts);
	std::vector<unsigned short> indices(3*numFaces);

	verts[0] = 0.f; verts[1] = 0.f; verts[2] = 1.f;		// index 0 = top
	verts[3] = 0.f; verts[4] = 0.f; verts[5] = 1.f;		
	verts[6] = 0.f; verts[7] = 0.f; verts[8] = -1.f;	// index 1 = bottom
	verts[9] = 0.f; verts[10] = 0.f; verts[11] = -1.f;	

	off = 2*6;
	for(m = 0; m < kM; ++m)
	{
		ASSERT(off < numVerts*6);
		float theta = 2 * M_PI * (m/(float)kM);
		float ctheta = cos(theta);
		float stheta = sin(theta);
		for(n = 0; n < kN-2; ++n, off += 6)
		{
			float phi = M_PI * (n+1) / (float)(kN);
			float cphi = cos(phi);
			float sphi = sin(phi);

			verts[off+0] = sphi * ctheta;
			verts[off+1] = sphi * stheta;
			verts[off+2] = cphi;
			verts[off+3] = sphi * ctheta;
			verts[off+4] = sphi * stheta;
			verts[off+5] = cphi;
		}
	}
	ASSERT(off == numVerts*6);

	off = 0;
	for(m = 0; m < kM; ++m)
	{
		ASSERT(off < numFaces*3);
		// top triangle
		indices[off++] = 0;
		// verts are arranged in columns, so get the start index of each column
		int col0 = 2 + (m * (kN-2));
		int col1 = 2 + (((m+1)%kM) * (kN-2));
		indices[off++] = col0;
		indices[off++] = col1;
		for(n = 0; n < (kN-3); ++n)
		{
			indices[off++] = col0 + n;
			indices[off++] = col0 + n + 1;
			indices[off++] = col1 + n;

			indices[off++] = col1 + n;
			indices[off++] = col0 + n + 1;
			indices[off++] = col1 + n + 1;
		}
		indices[off++] = col0 + (kN-3);
		indices[off++] = 1;
		indices[off++] = col1 + (kN-3);
	}
	ASSERT(off == numFaces*3);

	return std::make_shared<Geom>(
		numVerts, &verts[0],
		numFaces*3, &indices[0],
		6 * sizeof(float), GL_TRIANGLES,
		std::vector<GeomBindPair>{{GEOM_Pos, 3, 0}, {GEOM_Normal, 3, 3*sizeof(float)}}
	);
}

////////////////////////////////////////////////////////////////////////////////
void checkGlError(const char* str)
{	
	static char s_lastError[256];
	static int s_repeatCount;
	char curError[256];

	if(str == NULL)
		str = "";
	GLenum error = glGetError();
	if(error != GL_NO_ERROR)
	{
		snprintf(curError, sizeof(curError)-1, "%s: gl error: %s\n", str, gluErrorString(error));
		curError[sizeof(curError)-1] = '\0';
		if(strncmp(curError, s_lastError, sizeof(curError)-1) == 0) {
			++s_repeatCount;
			if(s_repeatCount == 1) 
				fputs("(error is repeating)\n", stderr);
		} else {
			s_repeatCount = 0;
			strncpy(s_lastError, curError, sizeof(s_lastError));
			fputs(curError, stderr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
static int render_CheckShaderCompile(GLuint sh)
{
	int status;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
	if(status == GL_FALSE)
	{
		char buf[1024];
		int len;
		printf("Failed to compile shader\n");
		glGetShaderSource(sh, sizeof(buf), &len, buf);
		buf[len-1] = '\0';
		printf("Source:\n%s\n", buf);
		glGetShaderInfoLog(sh, sizeof(buf), &len, buf);
		buf[len-1] = '\0';
		printf("%s\n", buf);
		return 0;
	}
	return 1;
}

static int render_CheckShaderLink(GLuint sh)
{
	int status;
	glGetProgramiv(sh, GL_LINK_STATUS, &status);
	if(status == GL_FALSE)
	{
		char buf[1024];
		int len;
		printf("Failed to link shader program\n");
		glGetProgramInfoLog(sh, sizeof(buf), &len, buf);
		buf[len-1] = '\0';
		printf("%s\n", buf);
		return 0;
	}
	return 1;
}

std::shared_ptr<ShaderInfo> render_CompileShader(const char* filename)
{
	std::shared_ptr<ShaderInfo> shader = std::make_shared<ShaderInfo>(filename);
	g_shaders.push_back(shader);
	shader->Recompile();
	return shader;
}

std::shared_ptr<ShaderInfo> render_CompileShader(const char* filename, 
	const std::vector<CustomShaderAttr>& customSpec)
{
	std::shared_ptr<ShaderInfo> shader = std::make_shared<ShaderInfo>(filename, customSpec);
	g_shaders.push_back(shader);
	shader->Recompile();
	return shader;
}

void render_RefreshShaders()
{
	std::cout << "Refreshing shaders... " << std::endl;
	for(auto shaderPtr : g_shaders)
		shaderPtr->Recompile();
}


////////////////////////////////////////////////////////////////////////////////
ShaderInfo::ShaderInfo(const std::string& filename)
	: m_program(0)
	, m_customSpec()
	, m_custom()
	, m_filename(filename)
{
	std::fill(m_uniforms,m_uniforms+BIND_NUM, -1);
	std::fill(m_attrs,m_attrs+GEOM_NUM, -1);
}

ShaderInfo::ShaderInfo(const std::string& filename, const std::vector<CustomShaderAttr>& customSpec)
	: m_program(0)
	, m_customSpec(customSpec)
				// size custom to be the equal to the largest id in customSpec
	, m_custom(
		std::minmax_element(customSpec.begin(), customSpec.end(), 
			[](const CustomShaderAttr& a, const CustomShaderAttr& b){return a.m_id < b.m_id;}).second->m_id + 1, -1)
	, m_filename(filename)
{
	std::fill(m_uniforms,m_uniforms+BIND_NUM, -1);
	std::fill(m_attrs,m_attrs+GEOM_NUM, -1);
}

ShaderInfo::~ShaderInfo()
{
	DeleteProgram();	
}

void ShaderInfo::FindCommonShaderLocs()
{
	GLuint p = m_program;

	GLint *uniforms = m_uniforms;
	uniforms[BIND_Mvp] = glGetUniformLocation(p, "mvp");
	uniforms[BIND_Model] = glGetUniformLocation(p, "model");
	uniforms[BIND_ModelIT] = glGetUniformLocation(p, "modelIT");
	uniforms[BIND_ModelInv] = glGetUniformLocation(p, "modelInv");
	uniforms[BIND_ModelView] = glGetUniformLocation(p, "modelView");
	uniforms[BIND_ModelViewIT] = glGetUniformLocation(p, "modelViewIT");
	uniforms[BIND_ModelViewInv] = glGetUniformLocation(p, "modelViewInv");
	uniforms[BIND_Sundir] = glGetUniformLocation(p, "sundir");
	uniforms[BIND_Color] = glGetUniformLocation(p, "color");
	uniforms[BIND_Eyepos] = glGetUniformLocation(p, "eyePos");
	uniforms[BIND_SunColor] = glGetUniformLocation(p, "sunColor");
	uniforms[BIND_Shininess] = glGetUniformLocation(p, "shininess");
	uniforms[BIND_Ambient] = glGetUniformLocation(p, "ambient");
	uniforms[BIND_Ka] = glGetUniformLocation(p, "Ka");
	uniforms[BIND_Kd] = glGetUniformLocation(p, "Kd");
	uniforms[BIND_Ks] = glGetUniformLocation(p, "Ks");

	GLint *attrs = m_attrs;
	attrs[GEOM_Pos] = glGetAttribLocation(p, "pos");
	if(attrs[GEOM_Pos] == -1) attrs[GEOM_Pos] = glGetAttribLocation(p, "position");
	attrs[GEOM_Uv] = glGetAttribLocation(p, "uv");
	attrs[GEOM_Normal] = glGetAttribLocation(p, "normal");
	attrs[GEOM_Color] = glGetAttribLocation(p, "color");
}

void ShaderInfo::CompileShaderChunks(const std::vector<ShaderChunk>& chunks)
{	
	if(chunks.empty()) return;
	int count = 2 + chunks.size() * 2;
	std::vector<const char*> sources(count);
	std::vector<GLint> lengths(count);

	sources[0] = kVersion130;
	lengths[0] = -1;
	sources[1] = "#define VERTEX_P\n";
	lengths[1] = -1;
	int idx = 2;
	for(const ShaderChunk& chunk : chunks)
	{
		sources[idx] = chunk.m_header.c_str();
		lengths[idx] = chunk.m_header.length();
		++idx;
		sources[idx] = chunk.m_source.c_str();
		lengths[idx] = chunk.m_source.length();
		++idx;
	}
	
	GLuint vtx = glCreateShader(GL_VERTEX_SHADER), 
		frag = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vtx, count, &sources[0], &lengths[0]);
	glCompileShader(vtx);
	if(!render_CheckShaderCompile(vtx))
		return;
	glAttachShader(m_program, vtx);

	sources[1] = "#define FRAGMENT_P\n";

	glShaderSource(frag, count, &sources[0], &lengths[0]);
	glCompileShader(frag);
	if(!render_CheckShaderCompile(frag))
		return;
	glAttachShader(m_program, frag);

	checkGlError("render_CompileShaderChunk");
}
		
ShaderInfo::ShaderChunk::ShaderChunk(const std::string& filename,
	int interruptedLine, const char* str, int strLen)
	: m_header()
	, m_source(str, strLen)
	, m_footer()
{
	std::stringstream hstr;
	std::stringstream fstr;
	hstr << std::endl << "#line 1 //" << filename << std::endl;
	fstr << std::endl << "#line " << interruptedLine << std::endl;
	m_header = hstr.str();
	m_footer = fstr.str();
}

void ShaderInfo::CompileShaderSources(const std::string& filename, std::vector<ShaderChunk>& chunks)
{
	std::fstream file(filename, std::ios_base::in | std::ios_base::binary);
	if(!file)
	{
		std::cerr << "Failed to open " << filename << std::endl;
		return;
	}

	file.seekg(0, std::ios_base::end);
	size_t size = file.tellg();
	file.seekg(0);

	std::vector<char> source(size+1);
	file.read(&source[0], size);
	source[source.size()-1] = '\0';

	if(file.fail()) {
		std::cerr << "Failed to read " << filename << std::endl;
		return;
	}
		
	int startpos = 0;
	int endpos = 0;

	int line = 0;
	while(source[endpos])
	{
		int cur = endpos;
		while(source[cur] && source[cur] != '#' && source[cur] != '\n') ++cur;
		if(source[cur] == '#')
		{ 
			if(strncasecmp("include", &source[cur+1], 7) == 0)
			{
				cur += 8; // skip '#include'
				// found an include directive
				if(endpos - startpos > 0)
					chunks.emplace_back(filename, line, &source[startpos], endpos - startpos);

				while(source[cur] && isspace(source[cur])) ++cur;
				int quotechar = source[cur];
				if(quotechar != '"') {
					std::cerr << filename << ':' << line << " bad #include line" << std::endl;
					break;
				}
				++cur;
				int filenameStart = cur;
				while(source[cur] && source[cur] != quotechar && source[cur] != '\n') ++cur;
				if(source[cur] == '\n') {
					std::cerr << filename << ':' << line << 
						" newline found before end of #include filename" << std::endl;
					break;
				}

				if(cur - filenameStart > 0)
				{
					std::string nextFile(&source[filenameStart], cur - filenameStart);
					CompileShaderSources(nextFile, chunks);
				}
				++cur; // skip quote char
				++cur; // skip newline
				startpos = cur;
			}
			else
			{
				while(source[cur] && source[cur] != '\n') ++cur;
			}
		}
		else if(source[cur])
			++cur; // skip newline
		endpos = cur; 
		++line;
	}

	if(endpos - startpos > 0)
		chunks.emplace_back(filename, line, &source[startpos], endpos - startpos);
}

// for demo version, make this load binaries from memory (glProgramBinary)

void ShaderInfo::DeleteProgram()
{
	// Detach previous shaders
	int totalShaders;
	glGetProgramiv(m_program, GL_ATTACHED_SHADERS, &totalShaders);
	std::vector<GLuint> shaders(totalShaders);
	GLsizei count;
	glGetAttachedShaders(m_program, totalShaders, &count, &shaders[0]);
	for(GLuint sh : shaders)
	{
		glDetachShader(m_program, sh);
		glDeleteShader(sh);
	}
	glDeleteProgram(m_program);
	m_program = 0;
}

void ShaderInfo::Recompile()
{
	if(m_program)
		DeleteProgram();

	m_program = glCreateProgram();

	std::vector<ShaderChunk> chunks;
	CompileShaderSources(m_filename, chunks);
	CompileShaderChunks(chunks);

	glLinkProgram(m_program);
	render_CheckShaderLink(m_program);

	for(int i = 0; i < BIND_NUM; ++i)
		m_uniforms[i] = -1;
	for(int i = 0; i < GEOM_NUM; ++i)
		m_attrs[i] = -1;

	FindCommonShaderLocs();

	for(int i = 0, c = m_customSpec.size(); i < c; ++i)
	{
		int idx = m_customSpec[i].m_id;
		m_custom[idx] = glGetUniformLocation(m_program, m_customSpec[i].m_name);
		if(m_custom[idx] < 0 && !m_customSpec[i].m_optional)
			std::cerr << "Failed to bind required uniform " << 
			m_customSpec[i].m_name << " in shader " << m_filename << std::endl;
	}
}

void render_SetTextureParameters(int sWrap, int tWrap, int magFilter, int minFilter)
{
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sWrap);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tWrap);
	checkGlError("render_SetTextureParameters");
}

////////////////////////////////////////////////////////////////////////////////
Framebuffer::Framebuffer(int width, int height)
	: m_fbo(0)
	, m_rboDepth(0)
	, m_hasStencil(false)
	, m_width(width)
	, m_height(height)
	, m_tbo()
{
}

Framebuffer::~Framebuffer()
{
	for(GLuint tex: m_tbo)
		glDeleteTextures(1, &tex);
	if(m_rboDepth)
		glDeleteRenderbuffers(1, &m_rboDepth);
	glDeleteFramebuffers(1, &m_fbo);
}

void Framebuffer::AddDepth(bool stencil)
{
	m_hasStencil = stencil;
	glGenRenderbuffers(1, &m_rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, m_rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, stencil ? GL_DEPTH_STENCIL : GL_DEPTH, m_width, m_height);
	checkGlError("Framebuffer::AddDepth");
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Framebuffer::AddTexture(int internalFormat, int format, int dataType)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	render_SetTextureParameters(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0, format, dataType, 0);
	m_tbo.push_back(tex);
	checkGlError("Framebuffer::AddTexture");
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Framebuffer::Create()
{	
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, m_hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER, m_rboDepth);
	for(int i = 0, c = m_tbo.size(); i < c; ++i)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_tbo[i], 0);
	GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Error: incomplete framebuffer" << std::endl;
	}
}

void Framebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void Framebuffer::CopyTexture(int index, GLuint destTex, int internalFormat) const
{
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glCopyTexImage2D(destTex, 0, internalFormat, 0, 0, m_width, m_height, 0);
	glReadBuffer(GL_BACK);
}

////////////////////////////////////////////////////////////////////////////////
ViewportState::ViewportState(int x, int y, int w, int h)
	: m_oldViewport()
{
	glGetIntegerv(GL_VIEWPORT, m_oldViewport);
	glViewport(x, y, w, h);
}

ViewportState::~ViewportState()
{
	glViewport(m_oldViewport[0], m_oldViewport[1], m_oldViewport[2], m_oldViewport[3]);
}

////////////////////////////////////////////////////////////////////////////////
ScissorState::ScissorState(int x, int y, int w, int h)
	: m_oldScissor()
	, m_prevEnabled(GL_FALSE)
{
	glGetIntegerv(GL_SCISSOR_BOX, m_oldScissor);
	glGetBooleanv(GL_SCISSOR_TEST, &m_prevEnabled);
	if(!m_prevEnabled)
		glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, w, h);
}

ScissorState::~ScissorState()
{
	glScissor(m_oldScissor[0], m_oldScissor[1], m_oldScissor[2], m_oldScissor[3]);
	if(!m_prevEnabled)
		glDisable(GL_SCISSOR_TEST);
}

