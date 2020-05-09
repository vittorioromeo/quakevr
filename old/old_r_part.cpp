#else
void R_DrawParticles()
{
    if(!r_particles.value)
    {
        return;
    }

    const auto up = vup * 1.5_qf;
    const auto right = vright * 1.5_qf;

    using namespace quake::util;

    // TODO VR: (P2) this could be optimized a lot
    // https://community.khronos.org/t/drawing-my-quads-faster/61312/2
    pMgr.forBuffers([&](gltexture_t* texture, const ImageData& imageData,
                        ParticleBuffer& pBuffer) {
        (void)imageData;

        GL_Bind(texture);
        glEnable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDepthMask(GL_FALSE); // johnfitz -- fix for particle z-buffer bug

        glBegin(GL_QUADS);
        pBuffer.forActive([&](PHandle p) {
            // johnfitz -- particle transparency and fade out
            GLubyte* c = (GLubyte*)&d_8to24table[(int)p.color()];

            GLubyte color[4];
            color[0] = c[0];
            color[1] = c[1];
            color[2] = c[2];
            color[3] = p.alpha() > 0 ? p.alpha() : 0;

            glColor4ubv(color);

            const auto xFwd = p.org() - r_origin;

            // TODO VR: (P2) `glm::rotate` is the bottleneck in debug mode (!)
            const auto xUp = glm::rotate(up, qfloat(p.angle()), xFwd);
            const auto xRight = glm::rotate(right, qfloat(p.angle()), xFwd);

            const auto halfScale = p.scale() / 2._qf;
            const auto xLeft = -xRight;
            const auto xDown = -xUp;
            const auto xUpLeft = p.org() + halfScale * xUp + halfScale * xLeft;
            const auto xUpRight =
                p.org() + halfScale * xUp + halfScale * xRight;
            const auto xDownLeft =
                p.org() + halfScale * xDown + halfScale * xLeft;
            const auto xDownRight =
                p.org() + halfScale * xDown + halfScale * xRight;

            glTexCoord2f(0, 0);
            glVertex3fv(toGlVec(xDownLeft));

            glTexCoord2f(1, 0);
            glVertex3fv(toGlVec(xUpLeft));

            glTexCoord2f(1, 1);
            glVertex3fv(toGlVec(xUpRight));

            glTexCoord2f(0, 1);
            glVertex3fv(toGlVec(xDownRight));
        });
        glEnd();

        glDepthMask(GL_TRUE); // johnfitz -- fix for particle z-buffer bug
        glDisable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1, 1, 1);
    });
}
